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

#define CLIP_FOR_BLIT() \
  int sl, st, sr, sb; \
  sl = sx; \
  st = sy; \
  sr = sx + sw - 1; \
  sb = sy + sh - 1; \
  if(dx < clip_left) { sl += clip_left - dx; dx = clip_left; } \
  if(dy < clip_top) { st += clip_top - dy; dy = clip_top; } \
  if(dx + (sr - sl) > clip_right) { sr += clip_right - (dx + (sr - sl)); } \
  if(dy + (sb - st) > clip_bottom) { sb += clip_bottom - (dy + (sb - st)); } \
  if(sl < 0) { dx -= sl; sl = 0; } \
  if(sr >= gfk->width) sr = gfk->width - 1; \
  if(st < 0) { dy -= st; st = 0; } \
  if(sb >= gfk->height) sb = gfk->height - 1; \
  if(sr < sl || sb < st) return;

static uint32_t clamp16(uint32_t x) {
  if(x > 65535) return 65535;
  else return x;
}

static uint32_t clamp16(float x) {
  if(x >= 65535.0f) return 65535;
  else return (uint32_t)x;
}

void Frisket::CopyFrisket(const Frisket*restrict gfk, int dx, int dy) restrict throw() {
  CopyFrisketRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Frisket::CopyFrisketRect(const Frisket*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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
  CLIP_FOR_BLIT();
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

void Drawable::Modulate(const Drawable*restrict gfk, int dx, int dy) restrict throw() {
  ModulateRect(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Drawable::ModulateRect(const Drawable*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  CLIP_FOR_BLIT();
  Pixel mask = 0xFF << ash;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    uint32_t r, g, b;
    Pixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
                r = ((uint32_t)SrgbToLinear[(*src >> rsh) & 255] *
                     (uint32_t)SrgbToLinear[(*dst >> rsh) & 255]) >> 16;
                g = ((uint32_t)SrgbToLinear[(*src >> gsh) & 255] *
                     (uint32_t)SrgbToLinear[(*dst >> gsh) & 255]) >> 16;
                b = ((uint32_t)SrgbToLinear[(*src >> bsh) & 255] *
                     (uint32_t)SrgbToLinear[(*dst >> bsh) & 255]) >> 16;
                *dst = (*dst & mask) | ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh);
                ++dst; ++src);
  }
}

void Drawable::Modulate2(const Drawable*restrict gfk, int dx, int dy) restrict throw() {
  ModulateRect2(gfk, 0, 0, gfk->width, gfk->height, dx, dy);
}

void Drawable::ModulateRect2(const Drawable*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw() {
  CLIP_FOR_BLIT();
  printf("%i,%i,%i,%i\n", sl, st, sr, sb);
  Pixel mask = 0xFF << ash;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    uint32_t r, g, b;
    Pixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
                r = clamp16(((uint32_t)SrgbToLinear[(*src >> rsh) & 255] *
                             (uint32_t)SrgbToLinear[(*dst >> rsh) & 255]) >> 15);
                g = clamp16(((uint32_t)SrgbToLinear[(*src >> gsh) & 255] *
                             (uint32_t)SrgbToLinear[(*dst >> gsh) & 255]) >> 15);
                b = clamp16(((uint32_t)SrgbToLinear[(*src >> bsh) & 255] *
                             (uint32_t)SrgbToLinear[(*dst >> bsh) & 255]) >> 15);
                *dst = (*dst & mask) | ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh);
                ++dst; ++src);
  }
}


void Drawable::ModulateF(const Drawable*restrict gfk, int dx, int dy, lua_Number f) restrict throw() {
  ModulateRectF(gfk, 0, 0, gfk->width, gfk->height, dx, dy, f);
}

void Drawable::ModulateRectF(const Drawable*restrict gfk, int sx, int sy, int sw, int sh, int dx, int dy, lua_Number f) restrict throw() {
  if(f == 1) return ModulateRect(gfk, sx, sy, sw, sh, dx, dy);
  else if(f == 2) return ModulateRect2(gfk, sx, sy, sw, sh, dx, dy);
  float ff = (float)f / 65535.0f;
  CLIP_FOR_BLIT();
  Pixel mask = 0xFF << ash;
  for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    uint32_t r, g, b;
    Pixel*restrict src, *restrict dst;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL_MORE(rem,
                r = clamp16(((uint32_t)SrgbToLinear[(*src >> rsh) & 255] *
                             (uint32_t)SrgbToLinear[(*dst >> rsh) & 255]) *
                            ff);
                g = clamp16(((uint32_t)SrgbToLinear[(*src >> gsh) & 255] *
                             (uint32_t)SrgbToLinear[(*dst >> gsh) & 255]) *
                            ff);
                b = clamp16(((uint32_t)SrgbToLinear[(*src >> bsh) & 255] *
                             (uint32_t)SrgbToLinear[(*dst >> bsh) & 255]) *
                            ff);
                *dst = (*dst & mask) | ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh);
                ++dst; ++src);
  }
}

int Drawable::Lua_Modulate(lua_State* L) restrict throw() {
  Drawable*restrict gfk = lua_toobject(L,1,Drawable);
  if(gfk == this) return luaL_error(L, "Source and destination Drawable must differ");
  if(gfk->layout != layout) {
    if(gfk->IsA("Graphic"))
      ((Graphic*)gfk)->ChangeLayout(layout);
    else if(IsA("Graphic"))
      ((Graphic*)this)->ChangeLayout(gfk->layout);
    else
      return luaL_error(L, "Attempt to modulate between two non-morphable Drawables!");
  }
  switch(lua_gettop(L)) {
  case 3: Modulate(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 4: ModulateF(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), luaL_checknumber(L, 4)); return 0;
  case 7: ModulateRect(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  case 8: ModulateRectF(gfk, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7), luaL_checknumber(L,8)); return 0;
  default:
    return luaL_error(L, "Modulate takes 3, 4, 7, or 8 parameters");
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
