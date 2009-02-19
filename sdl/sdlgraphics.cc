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

class EXPORT SDLGraphics : public GraphicsDevice {
 public:
  SDLGraphics(int width, int height, bool windowed, const char* title);
  virtual ~SDLGraphics();
  virtual void Update(int x, int y, int w, int h) throw();
  virtual void UpdateAll() throw();
  virtual int Lua_GetEvent(lua_State* L) throw();
  PROTOCOL_PROTOTYPE();
 private:
  int RealGetEvent(lua_State* L, bool wait, bool relmouse, bool textok) throw() LOCAL;
  SDL_Surface* screen;
  bool doing_relmouse, doing_textok;
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

SDLGraphics::SDLGraphics(int width, int height, bool windowed, const char* title) :
doing_relmouse(false), doing_textok(false) {
  this->width = width;
  this->height = height;
  InitializeSubsystem(SDL_INIT_VIDEO);
  Uint32 initflags = windowed ? 0 : SDL_FULLSCREEN;
  screen = SDL_SetVideoMode(width, height, 32, initflags);
  if(!screen && (initflags & SDL_HWSURFACE)) {
    initflags &= ~SDL_HWSURFACE;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  if(!screen && (initflags & SDL_FULLSCREEN)) {
    initflags &= ~SDL_FULLSCREEN;
    screen = SDL_SetVideoMode(width, height, 32, initflags);
  }
  if(!screen) {
    QuitSubsystem(SDL_INIT_VIDEO);
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
  SetupDrawable(screen->pixels, screen->pitch/sizeof(Pixel));
  SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
  SDL_EventState(SDL_VIDEORESIZE, SDL_IGNORE);
  SDL_ShowCursor(SDL_DISABLE);
  TryGammaCorrection();
}

void SDLGraphics::Update(int x, int y, int w, int h) throw() {
  //SDL_Rect r = {x, y, w, h};
  //SDL_BlitSurface(shadow, &r, screen, &r);
  SDL_UpdateRect(screen, x, y, w, h);
  // prevent asynchronous blitting as well as we can... this is needed on at
  // least one platform
  SDL_LockSurface(screen);
  SDL_UnlockSurface(screen);
}

void SDLGraphics::UpdateAll() throw() {
  //SDL_BlitSurface(shadow, NULL, screen, NULL);
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
      lua_pushnumber(L, evt.button.x);
      lua_setfield(L, -2, "x");
      lua_pushnumber(L, evt.button.y);
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

SDLGraphics::~SDLGraphics() {
  //SDL_FreeSurface(shadow);
  QuitSubsystem(SDL_INIT_VIDEO);
}

PROTOCOL_IMP_PLAIN(SDLGraphics, GraphicsDevice);

SUBCRITICAL_CONSTRUCTOR(SDLGraphics)(lua_State* L) {
  int width, height;
  bool windowed = false;
  const char* title = NULL;
  width = (int)luaL_checknumber(L, 1);
  height = (int)luaL_checknumber(L, 2);
  if(lua_istable(L, 3)) {
    lua_getfield(L, 3, "windowed");
    windowed = lua_toboolean(L, -1);
    lua_getfield(L, 3, "title");
    if(lua_isstring(L, -1)) title = lua_tostring(L, -1);
    lua_pop(L, 2);
  }
  try {
    SDLGraphics* ret = new SDLGraphics(width, height, windowed, title);
    ret->Push(L);
    return 1;
  }
  catch(const char* e) {
    return luaL_error(L, "Initialization failed: %s", e);
  }
}
