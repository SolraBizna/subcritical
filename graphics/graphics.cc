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
  METHOD("GetRealCount", &IndexArray::Lua_GetRealCount),
  METHOD("SetActiveRange", &IndexArray::Lua_SetActiveRange),
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
  clip_left = 0; clip_right = width - 1;
  clip_top = 0; clip_bottom = height - 1;
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

int Frisket::Lua_GetFrixel(lua_State* L) const throw() {
  lua_Integer x, y;
  x = luaL_checkinteger(L, 1);
  y = luaL_checkinteger(L, 2);
  if(x < 0 || x >= width || y < 0 || y >= height)
    return luaL_error(L, "coordinate out of range");
  lua_pushnumber(L, rows[y][x]/255.0);
  return 1;
}

int Frisket::Lua_GetRawFrixel(lua_State* L) const throw() {
  lua_Integer x, y;
  x = luaL_checkinteger(L, 1);
  y = luaL_checkinteger(L, 2);
  if(x < 0 || x >= width || y < 0 || y >= height)
    return luaL_error(L, "coordinate out of range");
  lua_pushnumber(L, rows[y][x]);
  return 1;
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
  METHOD("GetFrixel", &Frisket::Lua_GetFrixel),
  METHOD("GetRawFrixel", &Frisket::Lua_GetRawFrixel),
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
    // We now want to allow updating a zero-pixel region, so that the cursor
    // can be updated.
    //if(w <= 0 || h <= 0) return 0;
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

void Frisket::SetClipRect(int l, int t, int r, int b) throw() {
  clip_left = l;
  clip_top = t;
  clip_right = r;
  clip_bottom = b;
}

int Frisket::Lua_SetClipRect(lua_State* L) throw() {
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

void Frisket::GetClipRect(int& l, int& t, int& r, int& b) throw() {
  l = clip_left;
  t = clip_top;
  r = clip_right;
  b = clip_bottom;
}

int Frisket::Lua_GetClipRect(lua_State* L) throw() {
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

int Drawable::Lua_GetPixel(lua_State* L) const throw() {
  lua_Integer x, y;
  x = luaL_checkinteger(L, 1);
  y = luaL_checkinteger(L, 2);
  if(x < 0 || x >= width || y < 0 || y >= height)
    return luaL_error(L, "coordinate out of range");
  Pixel pixel = rows[y][x];
  switch(layout) {
  case FB_xRGB:
    lua_pushnumber(L, SrgbToLinear[(pixel>>16)&255]/65535.0);
    lua_pushnumber(L, SrgbToLinear[(pixel>>8)&255]/65535.0);
    lua_pushnumber(L, SrgbToLinear[pixel&255]/65535.0);
    lua_pushnumber(L, (pixel>>24)/255.0);
    break;
  case FB_RGBx:
    lua_pushnumber(L, SrgbToLinear[pixel>>24]/65535.0);
    lua_pushnumber(L, SrgbToLinear[(pixel>>16)&255]/65535.0);
    lua_pushnumber(L, SrgbToLinear[(pixel>>8)&255]/65535.0);
    lua_pushnumber(L, (pixel&255)/255.0);
    break;
  case FB_xBGR:
    lua_pushnumber(L, SrgbToLinear[pixel&255]/65535.0);
    lua_pushnumber(L, SrgbToLinear[(pixel>>8)&255]/65535.0);
    lua_pushnumber(L, SrgbToLinear[(pixel>>16)&255]/65535.0);
    lua_pushnumber(L, (pixel>>24)/255.0);
    break;
  case FB_BGRx:
    lua_pushnumber(L, SrgbToLinear[(pixel>>8)&255]/65535.0);
    lua_pushnumber(L, SrgbToLinear[(pixel>>16)&255]/65535.0);
    lua_pushnumber(L, SrgbToLinear[pixel>>24]/65535.0);
    lua_pushnumber(L, (pixel&255)/255.0);
    break;
  }
  return 4;
}

int Drawable::Lua_GetRawPixel(lua_State* L) const throw() {
  lua_Integer x, y;
  x = luaL_checkinteger(L, 1);
  y = luaL_checkinteger(L, 2);
  if(x < 0 || x >= width || y < 0 || y >= height)
    return luaL_error(L, "coordinate out of range");
  Pixel pixel = rows[y][x];
  switch(layout) {
  case FB_xRGB:
    lua_pushnumber(L, pixel);
    break;
  case FB_RGBx:
    lua_pushnumber(L, (pixel >> 8) | (pixel << 24));
    break;
  case FB_BGRx:
    lua_pushnumber(L, Swap32(pixel));
    break;
  case FB_xBGR:
    lua_pushnumber(L, Swap32((pixel << 8) | (pixel >> 24)));
    break;
  }
  return 1;
}

int Drawable::Lua_GetRawPixelNoAlpha(lua_State* L) const throw() {
  lua_Integer x, y;
  x = luaL_checkinteger(L, 1);
  y = luaL_checkinteger(L, 2);
  if(x < 0 || x >= width || y < 0 || y >= height)
    return luaL_error(L, "coordinate out of range");
  Pixel pixel = rows[y][x];
  switch(layout) {
  case FB_xRGB:
    lua_pushnumber(L, pixel & 0xFFFFFF);
    break;
  case FB_RGBx:
    lua_pushnumber(L, pixel >> 8);
    break;
  case FB_BGRx:
    lua_pushnumber(L, Swap32(pixel & 0xFFFFFF));
    break;
  case FB_xBGR:
    lua_pushnumber(L, Swap32(pixel << 8));
    break;
  }
  return 1;
}

int Drawable::Lua_GetRawAlpha(lua_State* L) const throw() {
  lua_Integer x, y;
  x = luaL_checkinteger(L, 1);
  y = luaL_checkinteger(L, 2);
  if(x < 0 || x >= width || y < 0 || y >= height)
    return luaL_error(L, "coordinate out of range");
  Pixel pixel = rows[y][x];
  switch(layout) {
  case FB_xRGB:
  case FB_xBGR:
    lua_pushnumber(L, pixel >> 24);
    break;
  case FB_RGBx:
  case FB_BGRx:
    lua_pushnumber(L, pixel & 255);
    break;
  }
  return 1;
}

int Drawable::Lua_GetSRGBPixel(lua_State* L) const throw() {
  lua_Integer x, y;
  x = luaL_checkinteger(L, 1);
  y = luaL_checkinteger(L, 2);
  if(x < 0 || x >= width || y < 0 || y >= height)
    return luaL_error(L, "coordinate out of range");
  Pixel pixel = rows[y][x];
  switch(layout) {
  case FB_xRGB:
    lua_pushnumber(L, ((pixel>>16)&255)/255.0);
    lua_pushnumber(L, ((pixel>>8)&255)/255.0);
    lua_pushnumber(L, (pixel&255)/255.0);
    lua_pushnumber(L, (pixel>>24)/255.0);
    break;
  case FB_RGBx:
    lua_pushnumber(L, (pixel>>24)/255.0);
    lua_pushnumber(L, ((pixel>>16)&255)/255.0);
    lua_pushnumber(L, ((pixel>>8)&255)/255.0);
    lua_pushnumber(L, (pixel&255)/255.0);
    break;
  case FB_xBGR:
    lua_pushnumber(L, (pixel&255)/255.0);
    lua_pushnumber(L, ((pixel>>8)&255)/255.0);
    lua_pushnumber(L, ((pixel>>16)&255)/255.0);
    lua_pushnumber(L, (pixel>>24)/255.0);
    break;
  case FB_BGRx:
    lua_pushnumber(L, ((pixel>>8)&255)/255.0);
    lua_pushnumber(L, ((pixel>>16)&255)/255.0);
    lua_pushnumber(L, (pixel>>24)/255.0);
    lua_pushnumber(L, (pixel&255)/255.0);
    break;
  }
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

IndexArray::IndexArray(size_t count) : count(count), real_count(count) {
  indices = (Index*)calloc(sizeof(Index), count);
  real_indices = indices;
}

int IndexArray::Lua_GetCount(lua_State* L) const throw() {
  lua_pushnumber(L, count);
  return 1;
}

int IndexArray::Lua_GetRealCount(lua_State* L) const throw() {
  lua_pushnumber(L, real_count);
  return 1;
}

int IndexArray::Lua_SetActiveRange(lua_State* L) throw() {
  lua_Integer first = luaL_checkinteger(L, 1);
  lua_Integer count = luaL_checkinteger(L, 2);
  if(first < 0 || count < 0) return luaL_error(L, "both parameters to SetActiveRange must be positive");
  SetActiveRange(first, count);
  return 0;
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
  int count = lua_rawlen(L, 1);
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
  int count = lua_rawlen(L, 1);
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

int GraphicsDevice::Lua_GetScreenModes(lua_State* L) throw() {
  lua_pushnil(L);
  lua_pushnil(L);
  return 2;
}

int GraphicsDevice::Lua_SetCursor(lua_State* L) throw() {
  if(lua_isnil(L,1)) SetCursor(NULL, 0, 0);
  else SetCursor(lua_toobject(L, 1, Graphic), luaL_optinteger(L,2,0), luaL_optinteger(L,3,0));
  return 0;
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
  METHOD("Modulate", &Drawable::Lua_Modulate),
  METHOD("Blit", &Drawable::Lua_Blit),
  METHOD("BlitFrisket", &Drawable::Lua_BlitFrisket),
  METHOD("TakeSnapshot", &Drawable::Lua_TakeSnapshot),
  METHOD("GetPixel", &Drawable::Lua_GetPixel),
  METHOD("GetRawPixel", &Drawable::Lua_GetRawPixel),
  METHOD("GetRawPixelNoAlpha", &Drawable::Lua_GetRawPixelNoAlpha),
  METHOD("GetRawAlpha", &Drawable::Lua_GetRawAlpha),
  METHOD("GetSRGBPixel", &Drawable::Lua_GetSRGBPixel),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Drawable, Object, DMethods);

static const struct ObjectMethod GDMethods[] = {
  METHOD("Update", &GraphicsDevice::Lua_Update),
  METHOD("GetEvent", &GraphicsDevice::Lua_GetEvent),
  METHOD("GetMousePos", &GraphicsDevice::Lua_GetMousePos),
  METHOD("GetScreenModes", &GraphicsDevice::Lua_GetScreenModes),
  METHOD("SetCursor", &GraphicsDevice::Lua_SetCursor),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(GraphicsDevice, Drawable, GDMethods);

SUBCRITICAL_CONSTRUCTOR(Graphic)(lua_State* L) {
  Drawable* d = NULL;
  if(lua_gettop(L) >= 3) { d = lua_toobject(L, 3, Drawable); }
  Graphic* p = new Graphic(luaL_checkinteger(L,1), luaL_checkinteger(L, 2), d ? d->layout : FB_RGBx);
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
