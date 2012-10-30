/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2012 Solra Bizna.

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

#if defined(__MACOSX__) || defined(__MACOS__)
#include <CoreServices/CoreServices.h>
#endif

using namespace SubCritical;

#if NO_OPENGL
#define CAN_DO_OPENGL 0
#else
#define CAN_DO_OPENGL 1
#include "SDL_opengl.h"
#endif

#if SHOULD_DO_SOFTSTRETCH // Don't enable this!
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
#else
#define CAN_DO_SOFTSTRETCH 0
#endif

#if !CAN_DO_SOFTSTRETCH
class DDA;
class DDAParams {
public:
  DDAParams(int num, int den) : num(num%den), den(den), out(num/den) {}
private:
  friend class DDA;
  int num, den, out;
};

class DDA {
public:
  DDA(const DDAParams& src) : run(0),tot(0) {}
  DDA(const DDA& src) : run(src.run),tot(src.tot) {}
  inline int Step(const DDAParams& src) {
    int ret = src.out;
    run += src.num;
    while(run >= src.den) {
      run -= src.den;
      ++ret;
    }
    tot += ret;
    return ret;
  }
  inline int Prestep(const DDAParams& src, int count) {
    run += count * src.num;
    tot += count * src.out;
    while(run >= src.den) {
      run -= src.den;
      ++tot;
    }
    return tot;
  }
  inline int GetTotal() { return tot; }
private:
  int run;
  int tot;
};

static void HardCoreSoftStretch(SDL_Surface* source, SDL_Rect* srcrect,
                                SDL_Surface* dest, SDL_Rect* dstrect) {
  // Output dest->w/source->w columns for each source column
  DDAParams out_x_params(dest->w, source->w);
  // Output dest->h/source->h rows for each source row
  DDAParams out_y_params(dest->h, source->h);

  DDA out_x_base(out_x_params);
  if(srcrect->x > 0) out_x_base.Prestep(out_x_params, srcrect->x);

  DDA out_y(out_y_params);
  if(srcrect->y > 0) out_y.Prestep(out_y_params, srcrect->y);

  const Pixel* inpp = (Pixel*)((uint8_t*)source->pixels + srcrect->y * source->pitch) + srcrect->x;
  Pixel* outpp = (Pixel*)((uint8_t*)dest->pixels + dstrect->y * dest->pitch) + dstrect->x;

  for(int y = 0; y < srcrect->h; ++y) {
    int outycount = out_y.Step(out_y_params);
    for(int n = 0; n < outycount; ++n) {
      const Pixel* inp = inpp;
      Pixel* outp = outpp;
      DDA out_x(out_x_base);
      for(int x = 0; x < srcrect->w; ++x) {
        int outxcount = out_x.Step(out_x_params);
        UNROLL_MORE(outxcount,
                    *outp++ = *inp;);
        ++inp;
      }
      outpp = (Pixel*)((uint8_t*)outpp + dest->pitch);
    }
    inpp = (const Pixel*)((uint8_t*)inpp + source->pitch);
  }
}
#endif

// This class is ugly.
// One dirty rect for the update rectangle, one for the cursor, and one for
// what was behind the cursor last update.
#define MAX_DIRTY_RECTS 3
struct LOCAL DirtyRects {
  static inline bool RectEatsWhole(const SDL_Rect& a, const SDL_Rect& b) {
    return b.x > a.x && b.y > a.y &&
      b.x + b.w <= a.x + a.w && b.y + b.h <= a.y + a.h;
  }
  inline DirtyRects() : count(0) {}
  inline void AddRect(int x, int y, int w, int h, SDL_Surface* who) {
    if(count >= MAX_DIRTY_RECTS) return; // silently drop!?
    if(x < 0) { w += x; x = 0; }
    if(y < 0) { h += y; y = 0; }
    if(x + w > who->w) w = who->w - x;
    if(y + h > who->h) h = who->h - y;
    if(w == 0 || h == 0) return;
    rects[count].x = x;
    rects[count].y = y;
    rects[count].w = w;
    rects[count].h = h;
    // if the sub-update falls entirely within the first update, do not waste
    // time updating any part of the screen twice
    // there are more cases of overdraw, but this one is the lowest-hanging
    // fruit
    if(count > 0 && RectEatsWhole(rects[0], rects[count])) return;
    ++count;
  }
  inline void Clear() { count = 0; }
  SDL_Rect rects[MAX_DIRTY_RECTS];
  int count;
};

class EXPORT SDLGraphics : public GraphicsDevice {
 public:
  SDLGraphics(int width, int height, bool windowed, const char* title,
              int true_width, int true_height, bool keep_aspect, bool smooth_filter, bool borderless);
  virtual ~SDLGraphics();
  virtual void Update(int x, int y, int w, int h) throw();
  virtual void UpdateAll() throw();
  virtual int Lua_GetEvent(lua_State* L) throw();
  virtual int Lua_GetMousePos(lua_State* L) throw();
  virtual int Lua_GetScreenModes(lua_State* L) throw();
  virtual void SetCursor(Graphic* cursor, int hx, int hy) throw();
  PROTOCOL_PROTOTYPE();
 private:
#if CAN_DO_OPENGL
  void GL_Update() throw() LOCAL;
#endif
  void UpdateOneRect(int x, int y, int w, int h) throw() LOCAL;
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
  DirtyRects target_dirty, screen_dirty;
  Graphic* cursor, *cbak, *old_cursor;
  int cursor_hx, cursor_hy;
  int cx, cy, old_cx, old_cy, old_cw, old_ch;
  static LOCAL Uint32 desktop_w, desktop_h;
};

LOCAL Uint32 SDLGraphics::desktop_w = 0;
LOCAL Uint32 SDLGraphics::desktop_h = 0;

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
  // before Snow Leopard, Macs came calibrated to 1.8 gamma
  // with Snow Leopard and later, autocalibration is disabled
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
#if defined(__MACOSX__) || defined(__MACOS__)
  // if we're running on 10.6 or later, disable automatic recalibration, since
  // it will probably do more harm than good and piss off people who calibrate
  // their displays themselves
  SInt32 major, minor;
  Gestalt(gestaltSystemVersionMajor, &major);
  Gestalt(gestaltSystemVersionMinor, &minor);
  if(major >= 11 || (major == 10 && minor >= 6)) return;
#endif
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
    //fprintf(stderr, "We could try to recover, but we won't.\n");
    exit(1);
  }
}
#define assertgl() _assertgl(__FILE__, __LINE__)
#endif

SDLGraphics::SDLGraphics(int width, int height, bool windowed, const char* title, int true_width, int true_height, bool keep_aspect, bool smooth_filter, bool borderless) :
  doing_relmouse(false), doing_textok(false), cursor(NULL), cbak(NULL), old_cursor(NULL), cx(0), cy(0) {
  SDLMan::current_screen = NULL;
  if(borderless) {
    // This is a destructive operation! Further windows will also be centered!
    // Unfortunately there is no SDL_unsetenv
    // This also potentially leaks a little memory...
    SDL_putenv(strdup("SDL_VIDEO_CENTERED=1"));
  }
  SDLMan::InitializeSubsystem(SDL_INIT_VIDEO);
  if(!desktop_w || !desktop_h) {
    const SDL_version* version = SDL_Linked_Version();
    if(version->major > 1 || (version->major == 1 && version->minor > 2) || (version->major == 1 && version->minor == 2 && version->patch >= 11)) {
      const SDL_VideoInfo* vi = SDL_GetVideoInfo();
      if(vi) {
        desktop_w = vi->current_w;
        desktop_h = vi->current_h;
      }
    }
  }
  if(width == 0) width = desktop_w ? desktop_w : 640;
  if(height == 0) height = desktop_h ? desktop_h : 480;
  Uint32 initflags = (windowed ? 0 : SDL_FULLSCREEN) | (borderless ? SDL_NOFRAME : 0);
  /* We will attempt upscaling only if requested. */
  bool tryFakeDoubling = false;
  if(((true_width != 0 && true_width != width) ||
      (true_height != 0 && true_height != height)))
    tryFakeDoubling = true;
#if CAN_DO_OPENGL
  /* Use OpenGL if upscaling is requested AND it is allowed. */
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
  /* Try again without OpenGL IF:
   * . Initializing with OpenGL failed.
   *  OR
   * . Initializing with OpenGL succeeded, but this OpenGL implementation does
   *   not support rectangular textures.
   */
  if((!screen && (initflags & SDL_OPENGL))
#if CAN_DO_OPENGL
     || ((initflags & SDL_OPENGL) && !haveRectTexture())
#endif
     ) {
    initflags &= ~SDL_OPENGL;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  /* Try again without hardware surfaces, if necessary. Currently, hardware
     surfaces are never requested. */
  if(!screen && (initflags & SDL_HWSURFACE)) {
    /* NOTREACHED */
    initflags &= ~SDL_HWSURFACE;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  /* Try again in windowed mode if we were attempting fullscreen. */
  if(!screen && (initflags & SDL_FULLSCREEN)) {
    initflags &= ~SDL_FULLSCREEN;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  /* Try again in with a frame if we were going without. (This is unlikely to
     work, but it's worth trying.) */
  if(!screen && (initflags & SDL_NOFRAME)) {
    initflags &= ~SDL_NOFRAME;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  /* If we still haven't gotten a screen, give up. */
  if(!screen) {
    SDLMan::QuitSubsystem(SDL_INIT_VIDEO);
    throw (const char*)SDL_GetError();
  }
  /* twm on X11 allows us to set the title before creating the window; indeed,
     this is actually helpful. However, on other platforms, setting the title
     first causes a crash. */
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
#else
     || tryFakeDoubling
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
#if CAN_DO_OPENGL
  if(screen->flags & SDL_OPENGL) {
    glViewport(0, 0, screen->w, screen->h);
  }
#endif
  SDLMan::current_screen = screen;
}

void SDLGraphics::SetCursor(Graphic* cursor, int hx, int hy) throw() {
  if(this->cbak && (!cursor || cursor->width != cbak->width || cursor->height != cbak->height)) {
    delete this->cbak;
    this->cbak = NULL;
  }
  this->cursor = cursor;
  if(cursor && !this->cbak)
    this->cbak = new Graphic(cursor->width, cursor->height, layout);
  this->cursor_hx = hx;
  this->cursor_hy = hy;
}

int SDLGraphics::Lua_GetScreenModes(lua_State* L) throw() {
  SDL_Rect** modes = SDL_ListModes(NULL, screen->flags | SDL_FULLSCREEN);
  if(modes == (SDL_Rect**)0 || modes == (SDL_Rect**)-1)
    lua_pushnil(L);
  else {
    lua_newtable(L);
    for(int n = 0; modes[n]; ++n) {
      lua_newtable(L);
      lua_pushinteger(L, modes[n]->w);
      lua_rawseti(L, -2, 1);
      lua_pushinteger(L, modes[n]->h);
      lua_rawseti(L, -2, 2);
      lua_rawseti(L, -2, n+1);
    }
  }
#if (SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION == 2 && SDL_PATCHLEVEL >= 11) || (SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION > 2) || (SDL_MAJOR_VERSION > 1)
  if(desktop_w && desktop_h) {
    lua_newtable(L);
    lua_pushinteger(L, desktop_w);
    lua_rawseti(L, -2, 1);
    lua_pushinteger(L, desktop_h);
    lua_rawseti(L, -2, 2);
  }
  else
#endif
    lua_pushnil(L);
  return 2;
}

#if CAN_DO_OPENGL
void SDLGraphics::GL_Update() throw() {
  // inefficient, whole-screen update; necessary because some hardware isn't
  // precise enough to get things right, and some drivers LIE about the number
  // of buffers they provide
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

void SDLGraphics::indirect_update(SDL_Surface* target, SDL_Rect& a,
                                  SDL_Surface* screen, SDL_Rect& b) throw() {
  if(false) {/* NOTREACHED */}
#if CAN_DO_OPENGL
  else if(screen->flags & SDL_OPENGL) {
    // Do this elsewhere.
    //glFinish();
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
  }
#endif
#if CAN_DO_SOFTSTRETCH
  else {
    /* This is wrong for partial updates on non-integer scale factors. I'm
       probably the only person who will ever use scaling with non-integer
       scale factors AND no OpenGL AND partial updates, so I don't care about
       fixing it. If so much as a single person asks me to actually fix this,
       I'll fix it. */
    /* Update: Someone did. So I did. */
    SDL_SoftStretch(target, &a, screen, &b);
    screen_dirty.AddRect(b.x, b.y, b.w, b.h, screen);
  }
#else
  else {
    HardCoreSoftStretch(target, &a, screen, &b);
    screen_dirty.AddRect(b.x, b.y, b.w, b.h, screen);
  }
#endif
}

void SDLGraphics::UpdateOneRect(int x, int y, int w, int h) throw() {
  if(screen != target) {
    SDL_Rect a = {(Sint16)x, (Sint16)y, (Uint16)w, (Uint16)h};
    SDL_Rect b;
    b.x = x * fake_rectangle.w / target->w;
    b.y = y * fake_rectangle.h / target->h;
    b.w = (x+w) * fake_rectangle.w / target->w - b.x;
    b.h = (y+h) * fake_rectangle.h / target->h - b.y;
    b.x += fake_rectangle.x;
    b.y += fake_rectangle.y;
    indirect_update(target, a, screen, b);
  }
  else screen_dirty.AddRect(x, y, w, h, screen);
}

void SDLGraphics::Update(int x, int y, int w, int h) throw() {
#if CAN_DO_OPENGL
  if(screen->flags & SDL_OPENGL) {
    // don't let the GL buffer more than one frame! (but allow it that much)
    glFinish();
    assertgl();
  }
#endif
  if(x < 0) { w += x; x = 0; }
  if(y < 0) { h += y; y = 0; }
  if(w + x > target->w) w = target->w - x;
  if(h + y > target->h) h = target->h - y;
  target_dirty.Clear();
  screen_dirty.Clear();
  if(w > 0 || h > 0)
    target_dirty.AddRect(x,y,w,h,target);
  int clip_l, clip_t, clip_r, clip_b, cbx = 0, cby = 0, cbw = 0, cbh = 0;
  if(cursor || old_cursor) {
    GetClipRect(clip_l, clip_t, clip_r, clip_b);
    SetClipRect(0, 0, width-1, height-1);
    if(cursor) {
      cbx = cx-cursor_hx; cby = cy-cursor_hy;
      cbw = cursor->width; cbh = cursor->height;
      cbak->CopyRect(this, cbx, cby, cbw, cbh, 0, 0);
      if(cursor->layout != layout) cursor->ChangeLayout(layout);
      Blit(cursor, cbx, cby);
      target_dirty.AddRect(cbx, cby, cbw, cbh, target);
    }
    if(old_cursor) {
      target_dirty.AddRect(old_cx, old_cy, old_cw, old_ch, target);
    }
  }
  if(target_dirty.count == 0) {
    // Nothing to update.
    goto skip_update;
  }
#if 0
  if(x == 0 && y == 0 && w == target->w && h == target->h) UpdateAll();
  else {
#endif
    for(int n = 0; n < target_dirty.count; ++n) {
      UpdateOneRect(target_dirty.rects[n].x, target_dirty.rects[n].y,
                    target_dirty.rects[n].w, target_dirty.rects[n].h);
    }
#if CAN_DO_OPENGL
    if(screen->flags & SDL_OPENGL)
      GL_Update();
    else
#endif
      if(screen_dirty.count > 0) {
      SDL_UpdateRects(screen, screen_dirty.count, screen_dirty.rects);
      // prevent asynchronous blitting as well as we can... this is needed on
      // at least one platform
      SDL_LockSurface(screen);
      SDL_UnlockSurface(screen);
    }
#if 0
  }
#endif
 skip_update:
  if(cursor || old_cursor) {
    SetClipRect(clip_l, clip_t, clip_r, clip_b);
    if(cursor) {
      Copy(cbak, cbx, cby);
      old_cx = cbx;
      old_cy = cby;
      old_cw = cbw;
      old_ch = cbh;
    }
    old_cursor = cursor;
  }
}

void SDLGraphics::UpdateAll() throw() {
#if 0
  // We used to have special code for this, for no particular performance gain,
  // but as Update() got more complicated, I decided to do away with this.
  //SDL_BlitSurface(shadow, NULL, screen, NULL);
  if(target != screen) {
    SDL_Rect a = {0, 0, (Uint16)target->w, (Uint16)target->h};
    SDL_Rect b = fake_rectangle;
    indirect_update(target, a, screen, b);
  }
  else
    SDL_Flip(screen);
  SDL_LockSurface(screen);
  SDL_UnlockSurface(screen);
#else
  Update(0, 0, width, height);
#endif
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
    {
      int report_x, report_y;
      if(relmouse) {
        report_x = evt.motion.xrel;
        report_y = evt.motion.yrel;
      }
      else if(target != screen) {
        report_x = (evt.motion.x-fake_rectangle.x)*target->w/screen->w;
        report_y = (evt.motion.y-fake_rectangle.y)*target->h/screen->h;
      }
      else {
        report_x = evt.motion.x;
        report_y = evt.motion.y;
      }
      if(relmouse) {
        cx += report_x;
        cy += report_y;
      }
      else {
        cx = report_x;
        cy = report_y;
      }
      lua_pushnumber(L, report_x);
      lua_setfield(L, -2, "x");
      lua_pushnumber(L, report_y);
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
      int report_x, report_y;
      if(target != screen) {
        report_x = (evt.button.x-fake_rectangle.x)*target->w/screen->w;
        report_y = (evt.button.y-fake_rectangle.y)*target->h/screen->h;
      }
      else {
        report_x = evt.button.x;
        report_y = evt.button.y;
      }
      cx = report_x;
      cy = report_y;
      lua_pushnumber(L, report_x);
      lua_setfield(L, -2, "x");
      lua_pushnumber(L, report_y);
      lua_setfield(L, -2, "y");
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
  int report_x, report_y;
  if(target != screen) {
    report_x = (x-fake_rectangle.x)*target->w/screen->w;
    report_y = (y-fake_rectangle.y)*target->h/screen->h;
  }
  else {
    report_x = x;
    report_y = y;
  }
  cx = report_x;
  cy = report_y;
  lua_pushnumber(L, report_x);
  lua_pushnumber(L, report_y);
  return 2;
}

SDLGraphics::~SDLGraphics() {
  //SDL_FreeSurface(shadow);
  if(target != screen)
    SDL_FreeSurface(target);
  // if SDLMan is ever made stackable, remove the following line
  if(screen == SDLMan::current_screen)
    SDLMan::QuitSubsystem(SDL_INIT_VIDEO);
}

PROTOCOL_IMP_PLAIN(SDLGraphics, GraphicsDevice);

SUBCRITICAL_CONSTRUCTOR(SDLGraphics)(lua_State* L) {
  int width, height;
  int true_width = 0, true_height = 0;
  bool windowed = false, keep_aspect = false, smooth_filter = false, borderless = false;
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
    lua_getfield(L, 3, "borderless");
    borderless = lua_toboolean(L,-1);
    lua_pop(L, 7);
  }
  try {
    SDLGraphics* ret = new SDLGraphics(width, height, windowed, title,
                                       true_width, true_height,
                                       keep_aspect, smooth_filter, borderless);
    ret->Push(L);
    return 1;
  }
  catch(const char* e) {
    return luaL_error(L, "Initialization failed: %s", e);
  }
}
