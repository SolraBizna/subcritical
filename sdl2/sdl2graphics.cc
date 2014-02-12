/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2014 Solra Bizna.

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

#include "sdl2man.h"
#include <math.h>
#include <assert.h>

#if defined(__MACOSX__) || defined(__MACOS__)
#define Fixed __Fixed__
#include <CoreServices/CoreServices.h>
#undef Fixed
#endif

using namespace SubCritical;

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
  inline void AddRect(int x, int y, int w, int h, int max_w, int max_h) {
    if(count >= MAX_DIRTY_RECTS) return; // silently drop!?
    if(x < 0) { w += x; x = 0; }
    if(y < 0) { h += y; y = 0; }
    if(x + w > max_w) w = max_w - x;
    if(y + h > max_h) h = max_h - y;
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

class EXPORT SDL2Graphics : public GraphicsDevice {
 public:
  SDL2Graphics(int width, int height, bool windowed, const char* title,
               int true_width, int true_height, bool keep_aspect,
               bool smooth_filter, bool borderless, bool vsync);
  virtual ~SDL2Graphics();
  virtual void Update(int x, int y, int w, int h) throw();
  virtual void UpdateAll() throw();
  virtual int Lua_GetEvent(lua_State* L) throw();
  virtual int Lua_GetMousePos(lua_State* L) throw();
  virtual int Lua_GetScreenModes(lua_State* L) throw();
  virtual void SetCursor(Graphic* cursor, int hx, int hy) throw();
  PROTOCOL_PROTOTYPE();
 private:
  void UpdateOneRect(int x, int y, int w, int h) throw() LOCAL;
  void indirect_update(SDL_Surface* source, SDL_Rect& a,
                       SDL_Surface* dest, SDL_Rect& b) throw() LOCAL;
  int RealGetEvent(lua_State* L, bool wait, bool relmouse, bool textok) throw() LOCAL;
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* framebuffer;
  bool doing_relmouse, doing_textok;
  DirtyRects dirty;
  Graphic* cursor, *cbak, *old_cursor;
  int cursor_hx, cursor_hy;
  int cx, cy, old_cx, old_cy, old_cw, old_ch;
  bool have_mouse_focus, had_mouse_focus, window_is_hiding;
  LOCAL static Uint32 desktop_w, desktop_h;
};

LOCAL Uint32 SDL2Graphics::desktop_w = 0;
LOCAL Uint32 SDL2Graphics::desktop_h = 0;

SDL2Graphics::SDL2Graphics(int width, int height, bool windowed, const char* title, int true_width, int true_height, bool keep_aspect, bool smooth_filter, bool borderless, bool vsync) :
doing_relmouse(false), doing_textok(false), cursor(NULL), cbak(NULL), old_cursor(NULL), cx(0), cy(0), have_mouse_focus(false), had_mouse_focus(false), window_is_hiding(false) {
  SDL2Man::current_screen = NULL;
  SDL2Man::InitializeSubsystem(SDL_INIT_VIDEO);
  if(desktop_w == 0) {
    SDL_Rect primary_display_size;
    if(SDL_GetDisplayBounds(0, &primary_display_size)) {
      fprintf(stderr, "Couldn't obtain primary display bounds, using 640x480.\n");
      primary_display_size = (SDL_Rect){0, 0, 640, 480};
    }
    desktop_w = primary_display_size.w;
    desktop_h = primary_display_size.h;
  }
  if(width == 0) width = desktop_w ? desktop_w : 640;
  if(height == 0) height = desktop_h ? desktop_h : 480;
  if(true_width == 0) true_width = width;
  if(true_height == 0) true_height = height;
  Uint32 initflags = 0;
  if(!windowed) initflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  if(borderless) {
    initflags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
    window_is_hiding = true;
  }
  window = SDL_CreateWindow(title,
                            borderless ? SDL_WINDOWPOS_CENTERED
                                       : SDL_WINDOWPOS_UNDEFINED,
                            borderless ? SDL_WINDOWPOS_CENTERED
                                       : SDL_WINDOWPOS_UNDEFINED,
                            width, height, initflags);
  /* Try again in windowed mode if we were attempting fullscreen. */
  if(!window && (initflags & SDL_WINDOW_FULLSCREEN)) {
    initflags &= ~SDL_WINDOW_FULLSCREEN_DESKTOP;
    window = SDL_CreateWindow(title,
                              borderless ? SDL_WINDOWPOS_CENTERED
                                         : SDL_WINDOWPOS_UNDEFINED,
                              borderless ? SDL_WINDOWPOS_CENTERED
                                         : SDL_WINDOWPOS_UNDEFINED,
                              width, height, initflags);
  }
  /* If we still haven't gotten a window, give up. */
  if(!window) {
    SDL2Man::QuitSubsystem(SDL_INIT_VIDEO);
    throw (const char*)SDL_GetError();
  }
  renderer = SDL_CreateRenderer(window, -1,
                                vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
  if(!renderer) {
    SDL_DestroyWindow(window);
    SDL2Man::QuitSubsystem(SDL_INIT_VIDEO);
    throw (const char*)SDL_GetError();
  }
  SDL_RenderClear(renderer);
  if(smooth_filter) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  SDL_RenderSetLogicalSize(renderer, true_width, true_height);
  framebuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  true_width, true_height);
  this->layout = FB_xRGB;
  this->width = true_width;
  this->height = true_height;
  SetupDrawable();
  SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
  SDL_ShowCursor(SDL_DISABLE);
  SDL2Man::current_screen = this;
}

void SDL2Graphics::SetCursor(Graphic* cursor, int hx, int hy) throw() {
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

int SDL2Graphics::Lua_GetScreenModes(lua_State* L) throw() {
  /* We are not going to actually implement this. Because SDL 2.0's fullscreen
     resolution switching support is buggy. Because apparently nobody wants it
     anymore. */
  lua_pushnil(L);
  if(desktop_w && desktop_h) {
    lua_newtable(L);
    lua_pushinteger(L, desktop_w);
    lua_rawseti(L, -2, 1);
    lua_pushinteger(L, desktop_h);
    lua_rawseti(L, -2, 2);
  }
  else
    lua_pushnil(L);
  return 2;
}

void SDL2Graphics::UpdateOneRect(int x, int y, int w, int h) throw() {
  // Causes video to stop displaying if scaling is used... sigh
  //SDL_Rect rect = {x, y, w, h};
  //SDL_UpdateTexture(framebuffer, &rect, rows[y] + x, (rows[1] - rows[0]) * sizeof(Pixel));
  SDL_UpdateTexture(framebuffer, NULL, buffer, (rows[1] - rows[0]) * sizeof(Pixel));
}

void SDL2Graphics::Update(int x, int y, int w, int h) throw() {
  if(x < 0) { w += x; x = 0; }
  if(y < 0) { h += y; y = 0; }
  if(w + x > width) w = width - x;
  if(h + y > height) h = height - y;
  dirty.Clear();
  if(w > 0 || h > 0)
    dirty.AddRect(x,y,w,h,width,height);
  int clip_l, clip_t, clip_r, clip_b, cbx = 0, cby = 0, cbw = 0, cbh = 0;
  if((have_mouse_focus && cursor) || (had_mouse_focus && old_cursor)) {
    GetClipRect(clip_l, clip_t, clip_r, clip_b);
    SetClipRect(0, 0, width-1, height-1);
    if(have_mouse_focus && cursor) {
      cbx = cx-cursor_hx; cby = cy-cursor_hy;
      cbw = cursor->width; cbh = cursor->height;
      cbak->CopyRect(this, cbx, cby, cbw, cbh, 0, 0);
      if(cursor->layout != layout) cursor->ChangeLayout(layout);
      Blit(cursor, cbx, cby);
      dirty.AddRect(cbx, cby, cbw, cbh, width, height);
    }
    if(had_mouse_focus && old_cursor) {
      dirty.AddRect(old_cx, old_cy, old_cw, old_ch, width, height);
    }
  }
  if(dirty.count == 0) {
    // Nothing to update.
    goto skip_update;
  }
  for(int n = 0; n < dirty.count; ++n) {
    UpdateOneRect(dirty.rects[n].x, dirty.rects[n].y,
                  dirty.rects[n].w, dirty.rects[n].h);
  }
  if(window_is_hiding) {
    SDL_ShowWindow(window);
    window_is_hiding = false;
  }
  SDL_RenderCopy(renderer, framebuffer, NULL, NULL);
  SDL_RenderPresent(renderer);
  had_mouse_focus = have_mouse_focus;
 skip_update:
  if((have_mouse_focus && cursor) || (had_mouse_focus && old_cursor)) {
    SetClipRect(clip_l, clip_t, clip_r, clip_b);
    if(have_mouse_focus && cursor) {
      Copy(cbak, cbx, cby);
      old_cx = cbx;
      old_cy = cby;
      old_cw = cbw;
      old_ch = cbh;
    }
    old_cursor = cursor;
  }
}

void SDL2Graphics::UpdateAll() throw() {
  Update(0, 0, width, height);
}

static const struct kmodpair {
  SDL_Keymod mod;
  const char* name;
} kmodpairs[] = {
  {(SDL_Keymod)KMOD_SHIFT, "shift"},
  {KMOD_LSHIFT, "lshift"},
  {KMOD_RSHIFT, "rshift"},
  {(SDL_Keymod)KMOD_CTRL, "control"},
  {KMOD_LCTRL, "lcontrol"},
  {KMOD_RCTRL, "rcontrol"},
  {(SDL_Keymod)KMOD_ALT, "alt"},
  {KMOD_LALT, "lalt"},
  {KMOD_RALT, "ralt"},
  {(SDL_Keymod)KMOD_GUI, "super"},
  {KMOD_LGUI, "lsuper"},
  {KMOD_RGUI, "rsuper"},
  {KMOD_NUM, "num"},
  {KMOD_CAPS, "capslock"},
};

static const struct keymap_pair {
  const char* old, *nu;
} keymap_pairs[] = {
  {"Left GUI", "left super"},
  {"Right GUI", "right super"},
};

/*
v = string.lower(k)
t[k] = v
return v
 */
static int KeyMapIndexFunction(lua_State* L) {
  lua_getglobal(L, "string");
  lua_getfield(L, -1, "lower");
  lua_pushvalue(L, 2);
  lua_call(L, 1, 1);
  lua_pushvalue(L, 2);
  lua_pushvalue(L, 4);
  lua_settable(L, 1);
  return 1;
}

static void PushKeyMapTable(lua_State* L) {
  lua_pushlightuserdata(L, (void*)SDL_GetKeyName);
  lua_gettable(L, LUA_REGISTRYINDEX);
  if(lua_isnil(L,-1)) {
    lua_pop(L, 1);
    lua_createtable(L, sizeof(keymap_pairs) / sizeof(*keymap_pairs), 0);
    for(unsigned n = 0; n < sizeof(keymap_pairs) / sizeof(*keymap_pairs); ++n) {
      lua_pushstring(L, keymap_pairs[n].old);
      lua_pushstring(L, keymap_pairs[n].nu);
      lua_settable(L, -3);
    }
    lua_createtable(L, 0, 1);
    /* Is it worth memoizing this? */
    lua_pushliteral(L, "__index");
    lua_pushcfunction(L, KeyMapIndexFunction);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_pushlightuserdata(L, (void*)SDL_GetKeyName);
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
}

int SDL2Graphics::RealGetEvent(lua_State* L, bool wait, bool relmouse, bool textok) throw() {
  if(!doing_relmouse && relmouse) {
    SDL_SetRelativeMouseMode(SDL_TRUE);
    doing_relmouse = true;
  }
  else if(doing_relmouse && !relmouse) {
    SDL_SetRelativeMouseMode(SDL_FALSE);
    doing_relmouse = false;
  }
  if(!doing_textok && textok) {
    SDL_StartTextInput();
    doing_textok = true;
  }
  else if(doing_textok && !textok) {
    SDL_StopTextInput();
    doing_textok = false;
  }
  SDL_Event evt;
  int suc;
  if(wait) suc = SDL_WaitEvent(&evt);
  else suc = SDL_PollEvent(&evt);
  if(!suc) return 0;
  switch(evt.type) {
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    lua_createtable(L, 0, 3);
    lua_pushstring(L, evt.key.state ? "keydown" : "keyup");
    lua_setfield(L, -2, "type");
    /* Errrrr... */
    PushKeyMapTable(L);
    lua_pushstring(L, SDL_GetKeyName(evt.key.keysym.sym));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pop(L, 1);
    /* /Errrrr... */
    lua_setfield(L, -2, "key");
    lua_createtable(L, 0, sizeof(kmodpairs)/sizeof(*kmodpairs));
    for(unsigned n = 0; n < sizeof(kmodpairs)/sizeof(*kmodpairs); ++n) {
      lua_pushboolean(L, evt.key.keysym.mod & kmodpairs[n].mod);
      lua_setfield(L, -2, kmodpairs[n].name);
    }
    lua_setfield(L, -2, "mod");
    break;
  case SDL_MOUSEMOTION:
    have_mouse_focus = true;
    lua_createtable(L, 0, 3);
    lua_pushliteral(L, "mousemove");
    lua_setfield(L, -2, "type");
    {
      int report_x, report_y;
      if(relmouse) {
        report_x = evt.motion.xrel;
        report_y = evt.motion.yrel;
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
  case SDL_MOUSEWHEEL:
    lua_createtable(L, 0, 3);
    lua_pushliteral(L, "scroll");
    lua_setfield(L, -2, "type");
    lua_pushnumber(L, evt.wheel.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, evt.wheel.y);
    lua_setfield(L, -2, "y");
    break;
  case SDL_MOUSEBUTTONDOWN:
    have_mouse_focus = true;
  case SDL_MOUSEBUTTONUP:
    lua_createtable(L, 0, relmouse ? 2 : 4);
    lua_pushstring(L, evt.button.state ? "mousedown" : "mouseup");
    lua_setfield(L, -2, "type");
    lua_pushnumber(L, evt.button.button);
    lua_setfield(L, -2, "button");
    if(!relmouse) {
      int report_x, report_y;
      report_x = evt.button.x;
      report_y = evt.button.y;
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
  case SDL_TEXTINPUT:
    lua_createtable(L, 0, 2);
    lua_pushliteral(L, "text");
    lua_setfield(L, -2, "type");
    lua_pushstring(L, evt.text.text);
    lua_setfield(L, -2, "text");
    break;
  case SDL_TEXTEDITING:
    /* TODO: figure out how this works */
    lua_createtable(L, 0, 4);
    lua_pushliteral(L, "textedit");
    lua_setfield(L, -2, "type");
    lua_pushstring(L, evt.edit.text);
    lua_setfield(L, -2, "text");
    lua_pushnumber(L, evt.edit.start);
    lua_setfield(L, -2, "start");
    lua_pushnumber(L, evt.edit.length);
    lua_setfield(L, -2, "length");
    break;
  default:
    return RealGetEvent(L, wait, relmouse, textok);
  }
  return 1;
}

int SDL2Graphics::Lua_GetEvent(lua_State* L) throw() {
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

int SDL2Graphics::Lua_GetMousePos(lua_State* L) throw() {
  int report_x, report_y;
  SDL_GetMouseState(&report_x, &report_y);
  cx = report_x;
  cy = report_y;
  lua_pushnumber(L, report_x);
  lua_pushnumber(L, report_y);
  return 2;
}

SDL2Graphics::~SDL2Graphics() {
  SDL_DestroyTexture(framebuffer);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  if(this == SDL2Man::current_screen)
    SDL2Man::QuitSubsystem(SDL_INIT_VIDEO);
}

PROTOCOL_IMP_PLAIN(SDL2Graphics, GraphicsDevice);

SUBCRITICAL_CONSTRUCTOR(SDL2Graphics)(lua_State* L) {
  int width, height;
  int true_width = 0, true_height = 0;
  bool windowed = false, keep_aspect = false, smooth_filter = false, borderless = false, vsync = false;
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
    lua_getfield(L, 3, "vsync");
    vsync = lua_toboolean(L,-1);
    lua_pop(L, 8);
  }
  try {
    SDL2Graphics* ret = new SDL2Graphics(width, height, windowed, title,
                                         true_width, true_height,
                                         keep_aspect, smooth_filter,
                                         borderless, vsync);
    ret->Push(L);
    return 1;
  }
  catch(const char* e) {
    return luaL_error(L, "Initialization failed: %s", e);
  }
}
