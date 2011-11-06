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
#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <math.h>

using namespace SubCritical;

#define PREFERRED_ALIGNMENT 1

Drawable::Drawable() : has_alpha(false),simple_alpha(true),buffer(NULL) {}

static const struct ObjectMethod CAMethods[] = {
  METHOD("GetCount", &CoordArray::Lua_GetCount),
  METHOD("GetCoord", &CoordArray::Lua_GetCoord),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(CoordArray, Object, CAMethods);
static const struct ObjectMethod IAMethods[] = {
  METHOD("GetCount", &IndexArray::Lua_GetCount),
  METHOD("GetIndex", &IndexArray::Lua_GetIndex),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(IndexArray, Object, IAMethods);

static int force_align(int width) {
  if(width % PREFERRED_ALIGNMENT == 0) return width;
  else return width - (width % PREFERRED_ALIGNMENT) + PREFERRED_ALIGNMENT;
}

int Drawable::PadWidth(int width) {
  return force_align(width);
}

Graphic::Graphic(int width, int height, FBLayout layout) {
  this->width = width;
  this->height = height;
  this->layout = layout;
  SetupDrawable();
}

Graphic::Graphic(Drawable& other) {
  this->width = other.width;
  this->height = other.height;
  this->layout = other.layout;
  SetupDrawable();
  this->has_alpha = other.has_alpha;
  this->simple_alpha = other.simple_alpha;
  if(other.fake_alpha) {
    Pixel mask;
    switch(layout) {
    default:
    case FB_RGBx:
    case FB_BGRx:
      mask = 0x000000FF; break;
    case FB_xRGB:
    case FB_xBGR:
      mask = 0xFF000000; break;
    }
    for(int y = 0; y < height; ++y) {
      Pixel*restrict src, *restrict dst;
      src = other.rows[y];
      dst = rows[y];
      size_t rem = width;
      UNROLL_MORE(rem,
		  *dst++ = *src++ | mask);
    }
  }
  else for(Pixel*restrict*restrict p = rows, *restrict*restrict q = other.rows; p - rows < height; ++p, ++q) {
    memcpy(*p, *q, width * sizeof(Pixel));
  }
}

Frisket::Frisket(int width, int height) : width(width), height(height) {
  int pitch = force_align(width) * sizeof(Pixel);
  buffer = (Frixel*)calloc(pitch, height + height * sizeof(Frixel*));
  rows = (Frixel*restrict*)(buffer + pitch * height);
  for(Frixel*restrict* p = rows; p - rows < height; ++p) {
    *p = buffer + pitch * (p - rows);
  }
}

static void EndianSwapPixels(Pixel* p, size_t rem) throw() {
  UNROLL_MORE(rem,
	      *p = (*p >> 24) | ((*p >> 8) & 0xFF00) | ((*p << 8) & 0xFF0000) | (*p << 24);
	      ++p;);
}

static void RotateRightPixels(Pixel* p, size_t rem) throw() {
  UNROLL_MORE(rem,
	      *p = (*p >> 8) | (*p << 24);
	      ++p;);
}

static void RotateLeftPixels(Pixel* p, size_t rem) throw() {
  UNROLL_MORE(rem,
	      *p = (*p << 8) | (*p >> 24);
	      ++p;);
}

static void SwapEvenComponentPixels(Pixel* p, size_t rem) throw() {
  UNROLL_MORE(rem,
	      *p = (*p & 0x00FF00FF) | ((*p >> 16) & 0x0000FF00) | ((*p << 16) & 0xFF000000);
	      ++p;);
}

static void SwapOddComponentPixels(Pixel* p, size_t rem) throw() {
  UNROLL_MORE(rem,
	      *p = (*p & 0xFF00FF00) | ((*p >> 16) & 0x000000FF) | ((*p << 16) & 0x00FF0000);
	      ++p;);
}

void Graphic::ChangeLayout(FBLayout nulayout) throw() {
  void(*func)(Pixel*, size_t);
  if(nulayout == layout) return;
  if(nulayout == (layout ^ FB_ENDIAN_SWAP_MASK)) func = EndianSwapPixels;
  else if((nulayout == FB_xRGB && layout == FB_RGBx) ||
	  (nulayout == FB_xBGR && layout == FB_BGRx)) func = RotateRightPixels;
  else if((nulayout == FB_RGBx && layout == FB_xRGB) ||
	  (nulayout == FB_BGRx && layout == FB_xBGR)) func = RotateLeftPixels;
  else if((nulayout == FB_BGRx && layout == FB_RGBx) ||
	  (nulayout == FB_RGBx && layout == FB_BGRx)) func = SwapEvenComponentPixels;
  else if((nulayout == FB_xBGR && layout == FB_xRGB) ||
	  (nulayout == FB_xRGB && layout == FB_xBGR)) func = SwapOddComponentPixels;
  else {
    fprintf(stderr, "Warning: Unknown layout switch path: %i -> %i\nLEAVING PIXEL DATA ALONE BUT SETTING THE NEW LAYOUT ANYWAY\n", layout, nulayout);
    layout = nulayout;
    return;
  }
  for(Pixel*restrict* p = rows; p < rows + height; ++p) func(*p, width);
  layout = nulayout;
  UpdateShifts();
}

void Graphic::CheckAlpha() throw() {
  Pixel mask;
  switch(layout) {
  default:
    fprintf(stderr, "WARNING: Checking alpha for unknown format %i!\n", layout);
  case FB_xRGB:
  case FB_xBGR:
    mask = 0xFF000000; break;
  case FB_RGBx:
  case FB_BGRx:
    mask = 0x000000FF; break;
  }
  has_alpha = false; // If we don't find alpha, there isn't any
  simple_alpha = true; // If we don't find partial alpha, there isn't any
  for(Pixel*restrict*restrict p = rows; p < rows + height; ++p) {
    Pixel*restrict q = *p;
    size_t rem = width;
    UNROLL(rem,
	   if((*q & mask) != mask) {
	     has_alpha = true;
	     if(*q & mask) { simple_alpha = false; return; }
	   }
	   ++q);
  }
  if(!has_alpha) simple_alpha = false;
}

int Drawable::Lua_GetSize(lua_State* L) const throw() {
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
}

int Frisket::Lua_GetSize(lua_State* L) const throw() {
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
}

int Graphic::Lua_OptimizeFor(lua_State* L) restrict throw() {
  Drawable*restrict dev = lua_toobject(L, 1, Drawable);
  if(dev == this) return luaL_error(L, "Source and destination Frisket must differ");
  ChangeLayout(dev->layout);
  return 0;
}

int Drawable::Lua_TakeSnapshot(lua_State* L) throw() {
  (new Graphic(*this))->Push(L);
  return 1;
}

Frisket::~Frisket() {
  free((void*)buffer);
}

static const struct ObjectMethod GMethods[] = {
  METHOD("OptimizeFor", &Graphic::Lua_OptimizeFor),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Graphic, Drawable, GMethods);

static const struct ObjectMethod FMethods[] = {
  METHOD("GetSize", &Frisket::Lua_GetSize),
  METHOD("CopyFrisket", &Frisket::Lua_CopyFrisket),
  METHOD("ModulateFrisket", &Frisket::Lua_ModulateFrisket),
  METHOD("AddFrisket", &Frisket::Lua_AddFrisket),
  METHOD("SubtractFrisket", &Frisket::Lua_SubtractFrisket),
  METHOD("MinFrisket", &Frisket::Lua_MinFrisket),
  METHOD("MaxFrisket", &Frisket::Lua_MaxFrisket),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Frisket, Object, FMethods);

void Drawable::SetupDrawable(bool invert_y) throw() {
  int pitch = force_align(width);
  buffer = (Pixel*)calloc(1, pitch * height * sizeof(Pixel) + height * sizeof(Pixel*));
  assert(buffer);
  rows = (Pixel*restrict*)(buffer + pitch * height);
  if(invert_y) for(Pixel*restrict* p = rows + height - 1; p >= rows; --p) {
    *p = buffer + pitch * (height - (p - rows) - 1);
  }
  else for(Pixel*restrict* p = rows; p - rows < height; ++p) {
    *p = buffer + pitch * (p - rows);
  }
  UpdateShifts();
  primitive_alpha = false;
  clip_left = 0; clip_right = width - 1;
  clip_top = 0; clip_bottom = height - 1;
  fake_alpha = !IsA("Graphic");
  merged_rows = true;
}

void Drawable::SetupDrawable(void* buffer, int32_t pitch) throw() {
  assert(force_align(pitch) == pitch);
  this->buffer = (Pixel*)buffer;
  rows = (Pixel**)calloc(sizeof(Pixel*), height);
  assert(rows);
  if(pitch < 0) for(Pixel*restrict* p = rows + height - 1; p >= rows; --p) {
    *p = this->buffer + -pitch * (height - (p - rows) - 1);
  }
  else for(Pixel*restrict* p = rows; p - rows < height; ++p) {
    *p = this->buffer + pitch * (p - rows);
  }
  UpdateShifts();
  primitive_alpha = false;
  clip_left = 0; clip_right = width - 1;
  clip_top = 0; clip_bottom = height - 1;
  fake_alpha = !IsA("Graphic");
  merged_rows = false;
}

void Drawable::UpdateShifts() {
  switch(layout) {
#define DO_FBLAYOUT(layout, _rsh, _gsh, _bsh, _ash, p)			\
    case layout: rsh = _rsh; gsh = _gsh; bsh = _bsh; ash = _ash; op_p = p; break
    DO_FBLAYOUT(FB_RGBx, 24, 16, 8, 0, 0xFFFFFF00);
    DO_FBLAYOUT(FB_xRGB, 16, 8, 0, 24, 0x00FFFFFF);
    DO_FBLAYOUT(FB_BGRx, 8, 16, 24, 0, 0xFFFFFF00);
  default:
    DO_FBLAYOUT(FB_xBGR, 0, 8, 16, 24, 0x00FFFFFF);
#undef DO_FBLAYOUT
  }
}

Drawable::~Drawable() {
  if(buffer) {
    if(merged_rows)
      free((void*)buffer);
    else {
      // buffer was allocated somewhere else
      Pixel** _rows = (Pixel**)rows; // work around restrict qualifier
      free((void*)_rows);
    }
  }
}

int GraphicsDevice::Lua_Update(lua_State* L) throw() {
  if(lua_gettop(L) == 0) UpdateAll();
  else {
    int x, y, w, h;
    x = (int)luaL_checkinteger(L, 1);
    y = (int)luaL_checkinteger(L, 2);
    w = (int)luaL_checkinteger(L, 3);
    h = (int)luaL_checkinteger(L, 4);
    if(x < 0) { w += x; x = 0; }
    if(x + w > width) w = width - x;
    if(y < 0) { h += y; y = 0; }
    if(y + h > height) h = height - y;
    if(w <= 0 || h <= 0) return 0;
    Update(x, y, w, h);
  }
  return 0;
}

void Drawable::SetClipRect(int l, int t, int r, int b) throw() {
  clip_left = l;
  clip_top = t;
  clip_right = r;
  clip_bottom = b;
}

int Drawable::Lua_SetClipRect(lua_State* L) throw() {
  int x, y, w, h;
  x = (int)luaL_checknumber(L, 1);
  y = (int)luaL_checknumber(L, 2);
  w = (int)luaL_checknumber(L, 3);
  h = (int)luaL_checknumber(L, 4);
  int l, t, r, b;
  l = x;
  t = y;
  r = x + w - 1;
  b = y + h - 1;
  if(l < 0) l = 0;
  if(t < 0) t = 0;
  if(r >= width) r = width - 1;
  if(b >= width) b = width - 1;
  if(r - l < 0 || b - t < 0) return luaL_error(L, "Invalid clip rect");
  SetClipRect(l, t, r, b);
  return 0;
}

void Drawable::GetClipRect(int& l, int& t, int& r, int& b) throw() {
  l = clip_left;
  t = clip_top;
  r = clip_right;
  b = clip_bottom;
}

int Drawable::Lua_GetClipRect(lua_State* L) throw() {
  int l, t, r, b;
  GetClipRect(l, t, r, b);
  int x, y, w, h;
  x = l;
  y = t;
  w = r - l + 1;
  h = b - t + 1;
  SetClipRect(l, t, r, b);
  lua_pushinteger(L, x);
  lua_pushinteger(L, y);
  lua_pushinteger(L, w);
  lua_pushinteger(L, h);
  return 4;
}

CoordArray::CoordArray(size_t count) : count(count) {
  coords = (Fixed*)calloc(sizeof(Fixed), count*2);
}

int CoordArray::Lua_GetCoord(lua_State* L) const throw() {
  lua_Integer index = luaL_checkinteger(L, 1);
  if(index < 0 || index >= (lua_Integer)count)
    return luaL_error(L, "index out of range");
  lua_pushnumber(L, Q_TO_F(coords[index*2]));
  lua_pushnumber(L, Q_TO_F(coords[index*2+1]));
  return 2;
}

int CoordArray::Lua_GetCount(lua_State* L) const throw() {
  lua_pushnumber(L, count);
  return 1;
}

CoordArray::~CoordArray() {
  free((void*)coords);
}

IndexArray::IndexArray(size_t count) : count(count) {
  indices = (Index*)calloc(sizeof(Index), count);
}

int IndexArray::Lua_GetCount(lua_State* L) const throw() {
  lua_pushnumber(L, count);
  return 1;
}

int IndexArray::Lua_GetIndex(lua_State* L) const throw() {
  lua_Integer index = luaL_checkinteger(L, 1);
  if(index < 0 || index >= (lua_Integer)count)
    return luaL_error(L, "index out of range");
  lua_pushnumber(L, indices[index]);
  return 1;
}

IndexArray::~IndexArray() {
  free((void*)indices);
}

SUBCRITICAL_UTILITY(CompileCoords)(lua_State* L) {
  if(!lua_istable(L,1)) return luaL_typerror(L, 1, "table");
  int count = lua_objlen(L, 1);
  if(count <= 0) return 0;
  count /= 2;
  CoordArray* ret = new CoordArray(count);
  count *= 2;
  for(int n = 0; n < count; ++n) {
    lua_rawgeti(L, 1, n + 1);
    if(!lua_isnumber(L, -1)) {
      delete ret;
      return luaL_error(L, "CompileCoords was given a table containing a non-number!");
    }
    ret->coords[n] = F_TO_Q(lua_tonumber(L,-1));
    lua_pop(L,1);
  }
  ret->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(CompileIndices)(lua_State* L) {
  if(!lua_istable(L,1)) return luaL_typerror(L, 1, "table");
  int count = lua_objlen(L, 1);
  if(count <= 0) return 0;
  IndexArray* ret = new IndexArray(count);
  for(int n = 0; n < count; ++n) {
    lua_rawgeti(L, 1, n + 1);
    if(!lua_isnumber(L, -1)) {
      delete ret;
      return luaL_error(L, "CompileIndices was given a table containing a non-number!");
    }
    ret->indices[n] = (int)lua_tonumber(L,-1);
    lua_pop(L,1);
  }
  ret->Push(L);
  return 1;
}

void Frisket::CopyFrisket(const Frisket*restrict gfk, int dx, int dy) restrict throw() {
  CopyFrisketRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Frisket::CopyFrisketRect(const Frisket*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Frixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    memcpy(dst, src, (sr - sl + 1) * sizeof(Frixel));
    //size_t rem = sr - sl + 1;
    //UNROLL_MORE(rem,
    //	*dst++ = *src++);
  }
}

void Frisket::ModulateFrisket(const Frisket*restrict gfk, int dx, int dy) restrict throw() {
  ModulateFrisketRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Frisket::ModulateFrisketRect(const Frisket*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Frixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
		*dst = (*src++ * *dst) / 255;
		++dst;);
  }
}

void Frisket::AddFrisket(const Frisket*restrict gfk, int dx, int dy) restrict throw() {
  AddFrisketRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Frisket::AddFrisketRect(const Frisket*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Frixel*restrict src, *restrict dst;
    int res;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
		res = *src++ + *dst;
		if(res > 255) *dst++ = 255;
		else *dst++ = res;);
  }
}

void Frisket::SubtractFrisket(const Frisket*restrict gfk, int dx, int dy) restrict throw() {
  SubtractFrisketRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Frisket::SubtractFrisketRect(const Frisket*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Frixel*restrict src, *restrict dst;
    int res;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
		res = *dst - *src++;
		if(res < 0) *dst++ = 0;
		else *dst++ = res;);
  }
}

void Frisket::MinFrisket(const Frisket*restrict gfk, int dx, int dy) restrict throw() {
  MinFrisketRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Frisket::MinFrisketRect(const Frisket*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Frixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
		*dst = *src > *dst ? *dst : *src;
		++src; ++dst;);
  }
}

void Frisket::MaxFrisket(const Frisket*restrict gfk, int dx, int dy) restrict throw() {
  MaxFrisketRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Frisket::MaxFrisketRect(const Frisket*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Frixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
		*dst = *src < *dst ? *dst : *src;
		++src; ++dst;);
  }
}

void Drawable::Copy(const Drawable*restrict gfk, int dx, int dy) restrict throw() {
  CopyRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Drawable::CopyRect(const Drawable*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(dx < clip_left) { sl += clip_left - dx; dx = clip_left; }
  if(dy < clip_top) { st += clip_top - dy; dy = clip_top; }
  if(dx + (sr - sl) > clip_right) { sr += clip_right - (dx + (sr - sl)); }
  if(dy + (sb - st) > clip_bottom) { sb += clip_bottom - (dy + (sb - st)); }
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  if(gfk->fake_alpha) {
    Pixel mask;
    switch(gfk->layout) {
    default:
    case FB_RGBx:
    case FB_BGRx:
      mask = 0x000000FF; break;
    case FB_xRGB:
    case FB_xBGR:
      mask = 0xFF000000; break;
    }
    for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
      Pixel*restrict src, *restrict dst;
      src = gfk->rows[sY] + sl;
      dst = rows[dY] + dx;
      size_t rem = sr - sl + 1;
      UNROLL_MORE(rem,
		  *dst++ = *src++ | mask);
    }
  }
  else for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Pixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    memcpy(dst, src, (sr - sl + 1) * sizeof(Pixel));
    //size_t rem = sr - sl + 1;
    //UNROLL_MORE(rem,
    //	*dst++ = *src++);
  }
  if(!fake_alpha) {
    if(!has_alpha && gfk->has_alpha) {
      has_alpha = true;
      simple_alpha = gfk->simple_alpha;
    }
    else if(has_alpha && simple_alpha) {
      simple_alpha = simple_alpha && gfk->simple_alpha;
    }
  }
}

void Drawable::Blit(const Drawable*restrict gfk, int dx, int dy) restrict throw() {
  BlitRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Drawable::BlitRect(const Drawable*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(dx < clip_left) { sl += clip_left - dx; dx = clip_left; }
  if(dy < clip_top) { st += clip_top - dy; dy = clip_top; }
  if(dx + (sr - sl) > clip_right) { sr += clip_right - (dx + (sr - sl)); }
  if(dy + (sb - st) > clip_bottom) { sb += clip_bottom - (dy + (sb - st)); }
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  if(gfk->has_alpha) {
    if(gfk->simple_alpha) {
      Pixel mask = 0xFF << ash;
      for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
	Pixel*restrict src, *restrict dst;
	src = gfk->rows[sY] + sl;
	dst = rows[dY] + dx;
	size_t rem = sr - sl + 1;
	UNROLL_MORE(rem,
		    if(*src & mask) *dst = *src;
		    ++dst; ++src);
      }
    }
    else for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
      Pixel*restrict src, *restrict dst;
      Pixel mask = 255 << ash;
      uint32_t r, g, b, a, ra;
      src = gfk->rows[sY] + sl;
      dst = rows[dY] + dx;
      size_t rem = sr - sl + 1;
      UNROLL(rem,
	     a = (*src >> ash) & 255;
	     a += a & 1;
	     ra = 256-a;
	     r = ((uint32_t)SrgbToLinear[(*src >> rsh) & 255] * a +
		  (uint32_t)SrgbToLinear[(*dst >> rsh) & 255] * ra) >> 8;
	     g = ((uint32_t)SrgbToLinear[(*src >> gsh) & 255] * a +
		  (uint32_t)SrgbToLinear[(*dst >> gsh) & 255] * ra) >> 8;
	     b = ((uint32_t)SrgbToLinear[(*src >> bsh) & 255] * a +
		  (uint32_t)SrgbToLinear[(*dst >> bsh) & 255] * ra) >> 8;
	     ++src;
	     *dst++ = ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh) | mask);
    }
  }
  else if(gfk->fake_alpha) {
    Pixel mask;
    switch(gfk->layout) {
    default:
    case FB_RGBx:
    case FB_BGRx:
      mask = 0x000000FF; break;
    case FB_xRGB:
    case FB_xBGR:
      mask = 0xFF000000; break;
    }
    for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
      Pixel*restrict src, *restrict dst;
      src = gfk->rows[sY] + sl;
      dst = rows[dY] + dx;
      size_t rem = sr - sl + 1;
      UNROLL_MORE(rem,
		  *dst++ = *src++ | mask);
    }
  }
  else for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Pixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    memcpy(dst, src, (sr - sl + 1) * sizeof(Pixel));
    //size_t rem = sr - sl + 1;
    //UNROLL_MORE(rem,
    //	*dst++ = *src++);
  }
}

void Drawable::BlitT(const Drawable*restrict gfk, int dx, int dy, lua_Number a) restrict throw() {
  BlitRectT(gfk, 0, 0, gfk->width, gfk->height, dx, dy, a);
}

void Drawable::BlitRectT(const Drawable*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy, lua_Number a) restrict throw() {
  uint32_t an = (uint32_t)floorf(a * 65536.f + 0.5f);
  if(an >= 65536) return BlitRect(gfk, sx, sy, sw, sh, dx, dy);
  else if(an == 0) return;
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(dx < clip_left) { sl += clip_left - dx; dx = clip_left; }
  if(dy < clip_top) { st += clip_top - dy; dy = clip_top; }
  if(dx + (sr - sl) > clip_right) { sr += clip_right - (dx + (sr - sl)); }
  if(dy + (sb - st) > clip_bottom) { sb += clip_bottom - (dy + (sb - st)); }
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  if(gfk->has_alpha) {
    if(gfk->simple_alpha) for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
      Pixel*restrict src, *restrict dst;
      Pixel mask = 0xFF << ash;
      uint32_t r, g, b, ra;
      src = gfk->rows[sY] + sl;
      dst = rows[dY] + dx;
      size_t rem = sr - sl + 1;
      ra = 65536 - an;
      UNROLL(rem,
	     if(*src & mask) {
	       r = ((uint32_t)SrgbToLinear[(*src >> rsh) & 255] * an +
		    (uint32_t)SrgbToLinear[(*dst >> rsh) & 255] * ra) >> 16;
	       g = ((uint32_t)SrgbToLinear[(*src >> gsh) & 255] * an +
		    (uint32_t)SrgbToLinear[(*dst >> gsh) & 255] * ra) >> 16;
	       b = ((uint32_t)SrgbToLinear[(*src >> bsh) & 255] * an +
		    (uint32_t)SrgbToLinear[(*dst >> bsh) & 255] * ra) >> 16;
	       *dst = ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh) | mask;
	     }
	     ++src; ++dst);
    }
    else for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
      Pixel*restrict src, *restrict dst;
      Pixel mask = 0xFF << ash;
      uint32_t r, g, b, a, ra;
      src = gfk->rows[sY] + sl;
      dst = rows[dY] + dx;
      size_t rem = sr - sl + 1;
      UNROLL(rem,
	     a = (*src >> ash) & 255;
	     a += a & 1;
	     a = (a * an) >> 8;
	     ra = 65536-a;
	     r = ((uint32_t)SrgbToLinear[(*src >> rsh) & 255] * a +
		  (uint32_t)SrgbToLinear[(*dst >> rsh) & 255] * ra) >> 16;
	     g = ((uint32_t)SrgbToLinear[(*src >> gsh) & 255] * a +
		  (uint32_t)SrgbToLinear[(*dst >> gsh) & 255] * ra) >> 16;
	     b = ((uint32_t)SrgbToLinear[(*src >> bsh) & 255] * a +
		  (uint32_t)SrgbToLinear[(*dst >> bsh) & 255] * ra) >> 16;
	     ++src;
	     *dst++ = ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh) | mask);
    }
  }
  else for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Pixel*restrict src, *restrict dst;
    uint32_t r, g, b, ra;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    ra = 65536 - an;
    UNROLL(rem,
	   r = ((uint32_t)SrgbToLinear[(*src >> rsh) & 255] * an +
		(uint32_t)SrgbToLinear[(*dst >> rsh) & 255] * ra) >> 16;
	   g = ((uint32_t)SrgbToLinear[(*src >> gsh) & 255] * an +
		(uint32_t)SrgbToLinear[(*dst >> gsh) & 255] * ra) >> 16;
	   b = ((uint32_t)SrgbToLinear[(*src >> bsh) & 255] * an +
		(uint32_t)SrgbToLinear[(*dst >> bsh) & 255] * ra) >> 16;
	   *dst = ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh);
	   ++src; ++dst);
    }
}

int Drawable::Lua_Blit(lua_State* L) restrict throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function. Try :Copy.");
  Drawable*restrict gfk = lua_toobject(L,1,Drawable);
  if(gfk == this) return luaL_error(L, "Source and destination Drawable must differ");
  if(gfk->layout != layout) {
    if(gfk->IsA("Graphic"))
      ((Graphic*)gfk)->ChangeLayout(layout);
    else if(IsA("Graphic"))
      ((Graphic*)this)->ChangeLayout(gfk->layout);
    else
      return luaL_error(L, "Attempt to blit between two non-morphable Drawables!");
  }
  switch(lua_gettop(L)) {
  case 3: Blit(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 4: BlitT(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), luaL_checknumber(L, 4)); return 0;
  case 7: BlitRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  case 8: BlitRectT(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7), luaL_checknumber(L,8)); return 0;
  default:
    return luaL_error(L, "Blit takes 3, 4, 7, or 8 parameters");
  }
}

int Drawable::Lua_Copy(lua_State* L) restrict throw() {
  Drawable*restrict gfk = lua_toobject(L,1,Drawable);
  if(gfk == this) return luaL_error(L, "Source and destination Drawable must differ");
  if(gfk->layout != layout) {
    if(gfk->IsA("Graphic"))
      ((Graphic*)gfk)->ChangeLayout(layout);
    else if(IsA("Graphic"))
      ((Graphic*)this)->ChangeLayout(gfk->layout);
    else
      return luaL_error(L, "Attempt to blit between two non-morphable Drawables!");
  }
  switch(lua_gettop(L)) {
  case 3: Copy(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: CopyRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "Copy takes 3 or 7 parameters");
  }
}

int Frisket::Lua_CopyFrisket(lua_State* L) restrict throw() {
  Frisket*restrict gfk = lua_toobject(L,1,Frisket);
  if(gfk == this) return luaL_error(L, "Source and destination Frisket must differ");
  switch(lua_gettop(L)) {
  case 3: CopyFrisket(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: CopyFrisketRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "CopyFrisket takes 3 or 7 parameters");
  }
}

int Frisket::Lua_ModulateFrisket(lua_State* L) restrict throw() {
  Frisket*restrict gfk = lua_toobject(L,1,Frisket);
  if(gfk == this) return luaL_error(L, "Source and destination Frisket must differ");
  switch(lua_gettop(L)) {
  case 3: ModulateFrisket(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: ModulateFrisketRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "ModulateFrisket takes 3 or 7 parameters");
  }
}

int Frisket::Lua_AddFrisket(lua_State* L) restrict throw() {
  Frisket*restrict gfk = lua_toobject(L,1,Frisket);
  if(gfk == this) return luaL_error(L, "Source and destination Frisket must differ");
  switch(lua_gettop(L)) {
  case 3: AddFrisket(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: AddFrisketRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "AddFrisket takes 3 or 7 parameters");
  }
}

int Frisket::Lua_SubtractFrisket(lua_State* L) restrict throw() {
  Frisket*restrict gfk = lua_toobject(L,1,Frisket);
  if(gfk == this) return luaL_error(L, "Source and destination Frisket must differ");
  switch(lua_gettop(L)) {
  case 3: SubtractFrisket(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: SubtractFrisketRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "SubtractFrisket takes 3 or 7 parameters");
  }
}

int Frisket::Lua_MinFrisket(lua_State* L) restrict throw() {
  Frisket*restrict gfk = lua_toobject(L,1,Frisket);
  if(gfk == this) return luaL_error(L, "Source and destination Frisket must differ");
  switch(lua_gettop(L)) {
  case 3: MinFrisket(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: MinFrisketRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "MinFrisket takes 3 or 7 parameters");
  }
}

int Frisket::Lua_MaxFrisket(lua_State* L) restrict throw() {
  Frisket*restrict gfk = lua_toobject(L,1,Frisket);
  if(gfk == this) return luaL_error(L, "Source and destination Frisket must differ");
  switch(lua_gettop(L)) {
  case 3: MaxFrisket(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: MaxFrisketRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "MaxFrisket takes 3 or 7 parameters");
  }
}

static const struct ObjectMethod DMethods[] = {
  METHOD("GetSize", &Drawable::Lua_GetSize),
  METHOD("GetClipRect", &Drawable::Lua_GetClipRect),
  METHOD("SetClipRect", &Drawable::Lua_SetClipRect),
  METHOD("SetPrimitiveColor", &Drawable::Lua_SetPrimitiveColor),
  METHOD("SetPrimitiveColorPremul", &Drawable::Lua_SetPrimitiveColorPremul),
  METHOD("DrawBox", &Drawable::Lua_DrawBox),
  METHOD("DrawRect", &Drawable::Lua_DrawRect),
  METHOD("DrawPoints", &Drawable::Lua_DrawPoints),
  METHOD("DrawLines", &Drawable::Lua_DrawLines),
  METHOD("DrawLineLoop", &Drawable::Lua_DrawLineLoop),
  METHOD("DrawLineStrip", &Drawable::Lua_DrawLineStrip),
  METHOD("DrawTriangles", &Drawable::Lua_DrawTriangles),
  METHOD("DrawTriangleStrip", &Drawable::Lua_DrawTriangleStrip),
  METHOD("DrawTriangleFan", &Drawable::Lua_DrawTriangleFan),
  METHOD("Copy", &Drawable::Lua_Copy),
  METHOD("Blit", &Drawable::Lua_Blit),
  METHOD("BlitFrisket", &Drawable::Lua_BlitFrisket),
  METHOD("TakeSnapshot", &Graphic::Lua_TakeSnapshot),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Drawable, Object, DMethods);

static const struct ObjectMethod GDMethods[] = {
  METHOD("Update", &GraphicsDevice::Lua_Update),
  METHOD("GetEvent", &GraphicsDevice::Lua_GetEvent),
  METHOD("GetMousePos", &GraphicsDevice::Lua_GetMousePos),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(GraphicsDevice, Drawable, GDMethods);

SUBCRITICAL_CONSTRUCTOR(Graphic)(lua_State* L) {
  Graphic* p = new Graphic(luaL_checkinteger(L,1), luaL_checkinteger(L, 2), FB_RGBx);
  p->has_alpha = false;
  p->simple_alpha = true;
  p->Push(L);
  return 1;
}

SUBCRITICAL_CONSTRUCTOR(Frisket)(lua_State* L) {
  Frisket* p = new Frisket(luaL_checkinteger(L,1), luaL_checkinteger(L, 2));
  p->Push(L);
  return 1;
}
