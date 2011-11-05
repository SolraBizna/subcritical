/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008 Solra Bizna.

  SubCritical is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 2 of the
  License, or (at your option) any later version.

  SubCritical is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of both the GNU General Public
  License and the GNU Lesser General Public License along with
  SubCritical.  If not, see <http://www.gnu.org/licenses/>.

  Please see doc/license.html for clarifications.
 */
#include "subcritical/graphics.h"

#include "sdlman.h"
#include <math.h>
#include <assert.h>

using namespace SubCritical;

#if !NO_OPENGL
#define CAN_DO_OPENGL 1
#include "SDL_opengl.h"
#else
#define CAN_DO_OPENGL 0
#endif

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2 && SDL_PATCHLEVEL >= 13
#define CAN_DO_SOFTSTRETCH 1
// OpenGL will always be preferred if available
static bool ShouldDoSoftStretch() {
  const SDL_version* version = SDL_Linked_Version();
  // 1.12 and earlier have a crash bug in them
  return version->major == 1 && version->minor == 2 && version->patch >= 13;
}
#else
// 1.3 doesn't have SoftStretch anymore, and it's not clear which version it
// was added in
#define CAN_DO_SOFTSTRETCH 0
#endif

class EXPORT SDLGraphics : public GraphicsDevice {
 public:
  SDLGraphics(int width, int height, bool windowed, const char* title,
              int true_width, int true_height, bool keep_aspect, bool smooth_filter);
  virtual ~SDLGraphics();
  virtual void Update(int x, int y, int w, int h) throw();
  virtual void UpdateAll() throw();
  virtual int Lua_GetEvent(lua_State* L) throw();
  virtual int Lua_GetMousePos(lua_State* L) throw();
  PROTOCOL_PROTOTYPE();
 private:
  void indirect_update(SDL_Surface* source, SDL_Rect& a,
                       SDL_Surface* dest, SDL_Rect& b) throw() LOCAL;
  int RealGetEvent(lua_State* L, bool wait, bool relmouse, bool textok) throw() LOCAL;
  SDL_Surface* screen;
  SDL_Surface* shadow; // used for SoftStretch and for OpenGL
  SDL_Surface* target; // either shadow or screen; the target of blitting
  SDL_Rect fake_rectangle; // only used if target != screen
  bool doing_relmouse, doing_textok;
#if CAN_DO_OPENGL
  GLenum glformat, gltype;
  int clear_count;
#endif
};

struct LOCAL gamma_frob {
  Uint16 old_ramp[256], new_ramp[256];
  float old_factor;
  void frobnicate(const char* envp);
};

static Uint16 table_get(double i, Uint16 t[256]) {
  double base = floor(i * 255.0);
  int i_base = (int)base;
  double more = i * 255.0 - base;
  if(more < 0.01f) return t[i_base];
  else {
    assert(i_base != 255);
    return (Uint16)floor(t[i_base] * (1.0 - more) + t[i_base+1] * more);
  }
}

void gamma_frob::frobnicate(const char* envp) {
  const char* env;
#if defined(__MACOSX__) || defined(__MACOS__)
  // Macintoshes have always come factory-calibrated to approximately 1.8 gamma
  // TODO: update this for Snow Leopard
  old_factor = 1.8;
#else
  // PC monitors are generally uncalibrated; CRTs natively have a gamma varying
  // between 2.2 and 2.6
  old_factor = 2.2;
#endif
  env = getenv(envp);
  if(!env) env = getenv("SCREEN_GAMMA");
  if(env) {
    char* e;
    double f = strtod(env, &e);
    if(e && *e) {
      fprintf(stderr, "Warning: Incomprehensible value given for one of the *_GAMMA environment variables.\n"
	      "Using the default of %.1f.\n", old_factor);
    }
    else old_factor = f;
  }
  double r_factor = 1.0 / old_factor;
  for(int n = 0; n < 256; ++n) {
    uint16_t lin = SrgbToLinear[n];
    double wrong = powf(lin/65535.0, r_factor);
    new_ramp[n] = table_get(wrong, old_ramp);
  }
  if(getenv("DEBUG_GAMMA")) {
    for(int n = 0; n < 256; ++n) {
      printf("%02X Old:%04X New:%04X\n", n, old_ramp[n], new_ramp[n]);
    }
  }
}

// Try, in as hardware-independent a fashion as possible, to set up proper
// gamma correction ramps for the display gamma. Try, also, to fail gracefully.
static void TryGammaCorrection() throw() {
  struct gamma_frob r, g, b;
  if(getenv("NO_FIX_GAMMA")) return;
  if(SDL_GetGammaRamp(r.old_ramp, g.old_ramp, b.old_ramp)) {
    fprintf(stderr, "Warning: Unable to get gamma ramps. Blending will look wrong.\n");
    return;
  }
  r.frobnicate("SCREEN_GAMMA_RED");
  g.frobnicate("SCREEN_GAMMA_GREEN");
  b.frobnicate("SCREEN_GAMMA_BLUE");
  if(SDL_SetGammaRamp(r.new_ramp, g.new_ramp, b.new_ramp)) {
    fprintf(stderr, "Warning: Unable to set gamma ramps. Blending will look wrong.\n");
    return;
  }
}

#if CAN_DO_OPENGL
/* http://www.mesa3d.org/brianp/sig97/exten.htm */
bool CheckExtension(const char* extName, const char* extensions) {
  /*
  ** Search for extName in the extensions string.  Use of strstr()
  ** is not sufficient because extension names can be prefixes of
  ** other extension names.  Could use strtok() but the constant
  ** string returned by glGetString can be in read-only memory.
  */
  const char* p = extensions;
  const char* end;
  int extNameLen;
  extNameLen = strlen(extName);
  end = p + strlen(p);
  while (p < end) {
    int n = strcspn(p, " ");
    if((extNameLen == n) && (strncmp(extName, p, n) == 0)) {
      return true;
    }
    p += (n + 1);
  }
  return false;
}

static bool haveRectTexture() {
  const char* exts = (const char*)glGetString(GL_EXTENSIONS);
  if(!exts) return false;
  return CheckExtension("GL_ARB_texture_rectangle",exts) || CheckExtension("GL_EXT_texture_rectangle",exts); // don't go with GL_NV_texture_rectangle.
}

static void _assertgl(const char* file, int line) {
  GLenum err;
  bool errored = false;
  while((err = glGetError()) != GL_NO_ERROR) {
    errored = true;
    fprintf(stderr, "GL error: %s\n", gluErrorString(err));
  }
  if(errored) {
    fprintf(stderr, "Fatal GL errors detected at: %s:%i\n", file, line);
    fprintf(stderr, "We could try to recover, but we won't.\n");
    exit(1);
  }
}
#define assertgl() _assertgl(__FILE__, __LINE__)
#endif

SDLGraphics::SDLGraphics(int width, int height, bool windowed, const char* title, int true_width, int true_height, bool keep_aspect, bool smooth_filter) :
doing_relmouse(false), doing_textok(false) {
  SDLMan::InitializeSubsystem(SDL_INIT_VIDEO);
  Uint32 initflags = windowed ? 0 : SDL_FULLSCREEN;
  bool tryFakeDoubling = false;
  if(((true_width != 0 && true_width != width) ||
      (true_height != 0 && true_height != height)))
    tryFakeDoubling = true;
#if CAN_DO_OPENGL
  if(tryFakeDoubling && !getenv("NO_OPENGL")) {
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    if(!getenv("ALLOW_SOFTWARE_OPENGL"))
      SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    initflags |= SDL_OPENGL;
  }
#endif
  screen = SDL_SetVideoMode(width, height, 32, initflags);
  if((!screen && (initflags & SDL_OPENGL))
#if CAN_DO_OPENGL
     || ((initflags & SDL_OPENGL) && !haveRectTexture())
#endif
     ) {
    initflags &= ~SDL_OPENGL;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  if(!screen && (initflags & SDL_HWSURFACE)) {
    /* NOTREACHED */
    initflags &= ~SDL_HWSURFACE;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  if(!screen && (initflags & SDL_FULLSCREEN)) {
    initflags &= ~SDL_FULLSCREEN;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  if(!screen) {
    SDLMan::QuitSubsystem(SDL_INIT_VIDEO);
    throw (const char*)SDL_GetError();
  }
  SDL_WM_SetCaption(title, title);
  // Determine the best layout to use.
  if(screen->format->BytesPerPixel != 4) throw (const char*)"Non-32-bit mode given";
  else switch(screen->format->Rmask) {
  // This code makes the assumption that other RGB layouts don't exist. I'm
  // crossing my fingers...
  case 0xFF000000: layout = FB_RGBx; break;
  case 0x00FF0000: layout = FB_xRGB; break;
  case 0x0000FF00: layout = FB_BGRx; break;
  case 0x000000FF: layout = FB_xBGR; break;
  default: throw (const char*)"Incompatible 32-bit mode given!";
  }
  Uint32 rmask, gmask, bmask;
  switch(layout) {
#define DO_FBLAYOUT(layout, rsh, gsh, bsh) \
  case layout: rmask = 0xFF << rsh; gmask = 0xFF << gsh; bmask = 0xFF << bsh; break
    DO_FBLAYOUT(FB_RGBx, 24, 16, 8);
  default:
    DO_FBLAYOUT(FB_xRGB, 16, 8, 0);
    DO_FBLAYOUT(FB_BGRx, 8, 16, 24);
    DO_FBLAYOUT(FB_xBGR, 0, 8, 16);
#undef DO_FBLAYOUT
  }
  assert((screen->pitch&3)==0);
  target = screen;
  if(false
#if CAN_DO_OPENGL
     || (screen->flags & SDL_OPENGL)
#endif
#if CAN_DO_SOFTSTRETCH
     || (tryFakeDoubling && ShouldDoSoftStretch())
#endif
       ) {
    shadow = SDL_CreateRGBSurface(0, true_width, true_height, 32, rmask, gmask, bmask, 0);
    if(!shadow)
      throw (const char*)SDL_GetError();
    assert((shadow->pitch&3)==0);
    target = shadow;
    if(!(screen->flags & SDL_OPENGL)) {
      /* clear boundaries, if any */
      SDL_FillRect(screen, NULL, 0);
      SDL_Flip(screen);
    }
  }
  if(target != screen) {
    if(keep_aspect) {
      if(screen->h * target->w > screen->w * target->h) {
        fake_rectangle.x = 0;
        fake_rectangle.w = screen->w;
        fake_rectangle.h = target->h * screen->w / target->w;
        fake_rectangle.y = (screen->h - fake_rectangle.h) / 2;
      }
      else {
        fake_rectangle.y = 0;
        fake_rectangle.h = screen->h;
        fake_rectangle.w = target->w * screen->h / target->h;
        fake_rectangle.x = (screen->w - fake_rectangle.w) / 2;
      }
    }
    else {
      fake_rectangle.x = 0;
      fake_rectangle.y = 0;
      fake_rectangle.w = screen->w;
      fake_rectangle.h = screen->h;
    }
  }
  this->width = target->w;
  this->height = target->h;
#if CAN_DO_OPENGL
  if(screen->flags & SDL_OPENGL) {
    glPixelStorei(GL_UNPACK_ROW_LENGTH, target->w);
    assertgl();
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    switch(this->layout) {
    case FB_xRGB:
      gltype = little_endian ? GL_UNSIGNED_INT_8_8_8_8 : GL_UNSIGNED_INT_8_8_8_8_REV;
      glformat = GL_BGRA;
      break;
    case FB_BGRx:
      gltype = little_endian ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_INT_8_8_8_8;
      glformat = GL_BGRA;
      break;
    case FB_xBGR:
      gltype = little_endian ? GL_UNSIGNED_INT_8_8_8_8 : GL_UNSIGNED_INT_8_8_8_8_REV;
      glformat = GL_RGBA;
      break;
    case FB_RGBx:
      gltype = little_endian ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_INT_8_8_8_8;
      glformat = GL_RGBA;
      break;
    }
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_RGB,target->w,target->h,
                 0,glformat,gltype,NULL);
    assertgl();
    /* we should handle an error here, really; that just involves shuffling
       this function a bit. the reason I haven't done it is that I think the
       failure case is REALLY unlikely. */
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, smooth_filter?GL_LINEAR:GL_NEAREST);
    assertgl();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    assertgl();
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    float u[4] = {(float)target->w/fake_rectangle.w, 0.f, 0.f, -(fake_rectangle.x*(float)target->w/fake_rectangle.w)};
    float v[4] = {0.f, -((float)target->h/fake_rectangle.h), 0.f, target->h+(fake_rectangle.y*(float)target->h/fake_rectangle.h)};
    glTexGenfv(GL_S, GL_OBJECT_PLANE, u);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, v);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    assertgl();
    glEnableClientState(GL_VERTEX_ARRAY);
    assertgl();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, screen->w, 0.0, screen->h);
    glMatrixMode(GL_MODELVIEW);
    assertgl();
    glClearColor(0.0, 0.0, 0.0, 0.0);
    clear_count = 0;
    assertgl();
  }
#endif
  SetupDrawable(target->pixels, target->pitch/sizeof(Pixel));
  SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
  SDL_EventState(SDL_VIDEORESIZE, SDL_IGNORE);
  SDL_ShowCursor(SDL_DISABLE);
#if HAVE_WINDOWS
  // ...
  (void)TryGammaCorrection;
#else
  TryGammaCorrection();
#endif
}

void SDLGraphics::indirect_update(SDL_Surface* target, SDL_Rect& a,
                                  SDL_Surface* screen, SDL_Rect& b) throw() {
  if(false) {/* NOTREACHED */}
#if CAN_DO_OPENGL
  else if(screen->flags & SDL_OPENGL) {
    glFinish();
    if(fake_rectangle.x != 0 || fake_rectangle.y != 0 ||
       fake_rectangle.w != screen->w || fake_rectangle.h != screen->h) {
      if(clear_count <= 0) {
        clear_count = 100;
        /* we SHOULD only need to do this once, but some drivers are screwy */
        glClear(GL_COLOR_BUFFER_BIT);
        a.x = 0;
        a.y = 0;
        a.w = target->w;
        a.h = target->h;
        b = fake_rectangle;
      } else --clear_count;
    }
    assertgl();
    if(a.x == 0 && a.y == 0 && a.w == target->w && a.h == target->h)
      glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGB, target->w, target->h,
                   0, glformat, gltype, target->pixels);
    else
      glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, a.x, a.y, a.w, a.h,
                      glformat, gltype, rows[a.y] + a.x);
    assertgl();
    GLushort rect[8] = {
      fake_rectangle.x, fake_rectangle.y,
      fake_rectangle.x+fake_rectangle.w, fake_rectangle.y,
      fake_rectangle.x+fake_rectangle.w, fake_rectangle.y+fake_rectangle.h,
      fake_rectangle.x, fake_rectangle.y+fake_rectangle.h,
    };
    glVertexPointer(2, GL_SHORT, 0, rect);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    SDL_GL_SwapBuffers();
    assertgl();
  }
#endif
#if CAN_DO_SOFTSTRETCH
  else {
    /* This is wrong for partial updates on non-integer scale factors. I'm
       probably the only person who will ever use scaling with non-integer
       scale factors AND no OpenGL AND partial updates, so I don't care about
       fixing it. If so much as a single person asks me to actually fix this,
       I'll fix it. */
    SDL_SoftStretch(target, &a, screen, &b);
    SDL_UpdateRect(screen, b.x, b.y, b.w, b.h);
  }
#else
  else {
    fprintf(stderr, "WARNING: target != screen and no suitable indirect update method is available! THIS SHOULD NEVER HAPPEN!\n");
  }
#endif
}

void SDLGraphics::Update(int x, int y, int w, int h) throw() {
  if(x < 0) { w += x; x = 0; }
  if(y < 0) { h += y; y = 0; }
  if(w + x > target->w) w = target->w - x;
  if(h + y > target->h) h = target->h - y;
  if(x == 0 && y == 0 && w == target->w && h == target->h) UpdateAll();
  else {
    if(screen != target) {
      SDL_Rect a = {x, y, w, h};
      SDL_Rect b;
      b.x = x * fake_rectangle.w / target->w;
      b.y = y * fake_rectangle.h / target->h;
      b.w = (x+w) * fake_rectangle.w / target->w - b.x;
      b.h = (y+h) * fake_rectangle.h / target->h - b.y;
      b.x += fake_rectangle.x;
      b.y += fake_rectangle.y;
      indirect_update(target, a, screen, b);
    }
    else SDL_UpdateRect(screen, x, y, w, h);
    // prevent asynchronous blitting as well as we can... this is needed on at
    // least one platform
    SDL_LockSurface(screen);
    SDL_UnlockSurface(screen);
  }
}

void SDLGraphics::UpdateAll() throw() {
  //SDL_BlitSurface(shadow, NULL, screen, NULL);
  if(target != screen) {
    SDL_Rect a = {0, 0, target->w, target->h};
    SDL_Rect b = fake_rectangle;
    indirect_update(target, a, screen, b);
  }
  else
    SDL_Flip(screen);
  SDL_LockSurface(screen);
  SDL_UnlockSurface(screen);
}

static const struct kmodpair {
  SDLMod mod;
  const char* name;
} kmodpairs[] = {
  {(SDLMod)KMOD_SHIFT, "shift"},
  {KMOD_LSHIFT, "lshift"},
  {KMOD_RSHIFT, "rshift"},
  {(SDLMod)KMOD_CTRL, "control"},
  {KMOD_LCTRL, "lcontrol"},
  {KMOD_RCTRL, "rcontrol"},
  {(SDLMod)KMOD_ALT, "alt"},
  {KMOD_LALT, "lalt"},
  {KMOD_RALT, "ralt"},
  {(SDLMod)KMOD_META, "meta"},
  {KMOD_LMETA, "lmeta"},
  {KMOD_RMETA, "rmeta"},
  {KMOD_NUM, "num"},
  {KMOD_CAPS, "capslock"},
};

int SDLGraphics::RealGetEvent(lua_State* L, bool wait, bool relmouse, bool textok) throw() {
  if(!doing_relmouse && relmouse) {
    SDL_WM_GrabInput(SDL_GRAB_ON);
    doing_relmouse = true;
  }
  else if(doing_relmouse && !relmouse) {
    SDL_WM_GrabInput(SDL_GRAB_OFF);
    doing_relmouse = false;
  }
  // all textok code will need to be updated for SDL 1.3
  if(!doing_textok && textok) {
    SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
    doing_textok = true;
  }
  else if(doing_textok && !textok) {
    SDL_EnableUNICODE(0);
    SDL_EnableKeyRepeat(0, 0);
    doing_textok = false;
  }
  SDL_Event evt;
  int suc;
  if(wait) suc = SDL_WaitEvent(&evt);
  else suc = SDL_PollEvent(&evt);
  if(!suc) return 0;
  switch(evt.type) {
  case SDL_KEYDOWN:
    if(textok && evt.key.keysym.unicode) {
      SDL_Event nuevt;
      nuevt.type = SDL_USEREVENT;
      nuevt.user.code = evt.key.keysym.unicode;
      SDL_PushEvent(&nuevt);
    }
  case SDL_KEYUP:
    lua_createtable(L, 0, 3);
    lua_pushstring(L, evt.key.state ? "keydown" : "keyup");
    lua_setfield(L, -2, "type");
    lua_pushstring(L, SDL_GetKeyName(evt.key.keysym.sym));
    lua_setfield(L, -2, "key");
    lua_createtable(L, 0, sizeof(kmodpairs)/sizeof(*kmodpairs));
    for(unsigned n = 0; n < sizeof(kmodpairs)/sizeof(*kmodpairs); ++n) {
      lua_pushboolean(L, evt.key.keysym.mod & kmodpairs[n].mod);
      lua_setfield(L, -2, kmodpairs[n].name);
    }
    lua_setfield(L, -2, "mod");
    break;
  case SDL_ACTIVEEVENT:
    if(evt.active.state == SDL_APPACTIVE) {
      lua_createtable(L, 0, 1);
      if(evt.active.gain)
	lua_pushliteral(L, "deiconify");
      else
	lua_pushliteral(L, "iconify");
      lua_setfield(L, -2, "type");
      break;
    } else return RealGetEvent(L, wait, relmouse, textok);
  case SDL_MOUSEMOTION:
    lua_createtable(L, 0, 3);
    lua_pushliteral(L, "mousemove");
    lua_setfield(L, -2, "type");
    if(relmouse) {
      lua_pushnumber(L, evt.motion.xrel);
      lua_setfield(L, -2, "x");
      lua_pushnumber(L, evt.motion.yrel);
      lua_setfield(L, -2, "y");
    }
    else if(target != screen) {
      lua_pushnumber(L, (evt.motion.x-fake_rectangle.x)*target->w/screen->w);
      lua_setfield(L, -2, "x");
      lua_pushnumber(L, (evt.motion.y-fake_rectangle.y)*target->h/screen->h);
      lua_setfield(L, -2, "y");
    }
    else {
      lua_pushnumber(L, evt.motion.x);
      lua_setfield(L, -2, "x");
      lua_pushnumber(L, evt.motion.y);
      lua_setfield(L, -2, "y");
    }
    break;
  case SDL_MOUSEBUTTONUP:
  case SDL_MOUSEBUTTONDOWN:
    lua_createtable(L, 0, relmouse ? 2 : 4);
    lua_pushstring(L, evt.button.state ? "mousedown" : "mouseup");
    lua_setfield(L, -2, "type");
    lua_pushnumber(L, evt.button.button);
    lua_setfield(L, -2, "button");
    if(!relmouse) {
      if(target != screen) {
        lua_pushnumber(L, (evt.button.x-fake_rectangle.x)*target->w/screen->w);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, (evt.button.y-fake_rectangle.y)*target->h/screen->h);
        lua_setfield(L, -2, "y");
      }
      else {
        lua_pushnumber(L, evt.button.x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, evt.button.y);
        lua_setfield(L, -2, "y");
      }
    }
    break;
    // TODO: joysticks
  case SDL_QUIT:
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "quit");
    lua_setfield(L, -2, "type");
    break;
  case SDL_USEREVENT:
    lua_createtable(L, 0, 2);
    lua_pushliteral(L, "text");
    lua_setfield(L, -2, "type");
    {
      // Get the UTF-8 string corresponding to the possible Unicode code point
      // SDL gave us.
      char buf[4];
      int c = evt.user.code;
      if(c < 0x80) {
	buf[0] = c;
	lua_pushlstring(L, buf, 1);
      }
      else if(c < 0x800) {
	buf[0] = 0xC0 | (c >> 6);
	buf[1] = 0x80 | (c & 0x3F);
	lua_pushlstring(L, buf, 2);
      }
      // values past here won't be given to us, at least by SDL 1.2
      else if(c < 0x10000) {
	buf[0] = 0xE0 | (c >> 12);
	buf[1] = 0x80 | ((c >> 6) & 0x3F);
	buf[2] = 0x80 | (c & 0x3F);
	lua_pushlstring(L, buf, 3);
      }
      else if(c < 0x110000) {
	buf[0] = 0xF0 | (c >> 18);
	buf[1] = 0x80 | ((c >> 12) & 0x3F);
	buf[2] = 0x80 | ((c >> 6) & 0x3F);
	buf[3] = 0x80 | (c & 0x3F);
	lua_pushlstring(L, buf, 4);
      }
      else {
	// Not a valid Unicode code point! Forget the whole thing.
	lua_pop(L, 1);
	return RealGetEvent(L, wait, relmouse, textok);
      }
    }
    lua_setfield(L, -2, "text");
    break;
  case SDL_VIDEOEXPOSE:
    UpdateAll();
    // fall through to next case
  default:
    return RealGetEvent(L, wait, relmouse, textok);
  }
  return 1;
}

int SDLGraphics::Lua_GetEvent(lua_State* L) throw() {
  bool wait = false, relmouse = false, textok = false;
  if(lua_istable(L, 1)) {
    lua_getfield(L, 1, "wait");
    wait = lua_toboolean(L, -1);
    lua_getfield(L, 1, "relmouse");
    relmouse = lua_toboolean(L, -1);
    lua_getfield(L, 1, "textok");
    textok = lua_toboolean(L, -1);
  }
  lua_pop(L, lua_gettop(L));
  return RealGetEvent(L, wait, relmouse, textok);
}

int SDLGraphics::Lua_GetMousePos(lua_State* L) throw() {
  int x, y;
  SDL_GetMouseState(&x, &y);
  if(target != screen) {
    lua_pushnumber(L, (x-fake_rectangle.x)*target->w/screen->w);
    lua_pushnumber(L, (y-fake_rectangle.y)*target->h/screen->h);
  }
  else {
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
  }
  return 2;
}

SDLGraphics::~SDLGraphics() {
  //SDL_FreeSurface(shadow);
  SDLMan::QuitSubsystem(SDL_INIT_VIDEO);
}

PROTOCOL_IMP_PLAIN(SDLGraphics, GraphicsDevice);

SUBCRITICAL_CONSTRUCTOR(SDLGraphics)(lua_State* L) {
  int width, height;
  int true_width = 0, true_height = 0;
  bool windowed = false, keep_aspect = false, smooth_filter = false;
  const char* title = NULL;
  width = (int)luaL_checknumber(L, 1);
  height = (int)luaL_checknumber(L, 2);
  if(lua_istable(L, 3)) {
    lua_getfield(L, 3, "windowed");
    windowed = lua_toboolean(L, -1);
    lua_getfield(L, 3, "title");
    if(lua_isstring(L, -1)) title = lua_tostring(L, -1);
    lua_getfield(L, 3, "true_width");
    true_width = luaL_optinteger(L, -1, 0);
    lua_getfield(L, 3, "true_height");
    true_height = luaL_optinteger(L, -1, 0);
    lua_getfield(L, 3, "keep_aspect");
    keep_aspect = lua_toboolean(L,-1);
    lua_getfield(L, 3, "smooth_filter");
    smooth_filter = lua_toboolean(L,-1);
    lua_pop(L, 6);
  }
  try {
    SDLGraphics* ret = new SDLGraphics(width, height, windowed, title,
                                       true_width, true_height,
                                       keep_aspect, smooth_filter);
    ret->Push(L);
    return 1;
  }
  catch(const char* e) {
    return luaL_error(L, "Initialization failed: %s", e);
  }
}
