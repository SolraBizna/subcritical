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

#include <math.h>

using namespace SubCritical;

SUBCRITICAL_UTILITY(ScaleFast)(lua_State* L) {
  Drawable*restrict source = lua_toobject(L, 1, Drawable);
  Graphic*restrict dest = new Graphic(luaL_checkinteger(L, 2), luaL_checkinteger(L, 3), source->layout);
  if(source == dest) return luaL_error(L, "source and destination must differ");
  dest->has_alpha = source->has_alpha;
  dest->simple_alpha = source->simple_alpha;
  Pixel*restrict*restrict srows = source->rows;
  int syn = source->height, syd = dest->height, sye = 0, sys;
  int sxn = source->width, sxd = dest->width, sxs;
  sys = syn / syd;
  syn = syn % syd;
  sxs = sxn / sxd;
  sxn = sxn % sxd;
  for(int y = 0; y < dest->height; ++y) {
    Pixel*restrict d = dest->rows[y];
    Pixel*restrict s = *srows;
    int sxe = 0;
    int rem = dest->width;
    UNROLL(rem,
	   *d++ = *s;
	   s += sxs;
	   sxe += sxn;
	   if(sxe >= sxd) {
	     sxe -= sxd;
	     ++s;
	   });
    srows += sys;
    sye += syn;
    if(sye >= syd) {
      sye -= syd;
      ++srows;
    }
  }
  dest->Push(L);
  return 1;
}

// ScaleBest was repurposed from sis4
#define FPI 3.141592653589793f
static inline float l3(float x) {
  if(x == 0.f) return 1.f;
  else return (sinf(FPI * x) / (FPI * x)) * (sinf(FPI * x / 3.f) / (FPI * x / 3.f));
}

#define L3SINC_TABLE_SIZE 65
#define L3SINC_TABLE_CENTER 32
#define L3SINC_TABLE_EDGE 33
#define L3SINC_TABLE_DIV ((L3SINC_TABLE_CENTER+1)/3.f)
static float l3sincf_table[L3SINC_TABLE_SIZE];
LUA_EXPORT int Init_effects(lua_State* L) {
  for(int y = 0; y < L3SINC_TABLE_SIZE; ++y) {
    l3sincf_table[y] = l3((y - L3SINC_TABLE_CENTER) / L3SINC_TABLE_DIV);
  }
  return 0;
}
//#define L3(x) l3sincf_table[(int)(x * L3SINC_TABLE_DIV + L3SINC_TABLE_CENTER)];
static float L3(float x) {
  int y = (int)(x * L3SINC_TABLE_DIV + L3SINC_TABLE_CENTER);
  // I don't understand why this was needed in SubCritical but not sis4.
  return y >= L3SINC_TABLE_SIZE ? 0.f : l3sincf_table[y];
}

SUBCRITICAL_UTILITY(ScaleBest)(lua_State* L) {
  Drawable*restrict source = lua_toobject(L, 1, Drawable);
  Graphic*restrict dest = new Graphic(luaL_checkinteger(L, 2), luaL_checkinteger(L, 3), source->layout);
  if(source == dest) return luaL_error(L, "source and destination must differ");
  int rsh, gsh, bsh, ash;
  switch(source->layout) {
#define DO_FBLAYOUT(layout, _rsh, _gsh, _bsh, _ash)			\
    case layout: rsh = _rsh; gsh = _gsh; bsh = _bsh; ash = _ash; break
    DO_FBLAYOUT(FB_RGBx, 24, 16, 8, 0);
    DO_FBLAYOUT(FB_xRGB, 16, 8, 0, 24);
    DO_FBLAYOUT(FB_BGRx, 8, 16, 24, 0);
  default:
    DO_FBLAYOUT(FB_xBGR, 0, 8, 16, 24);
#undef DO_FBLAYOUT
  }
  float xfact, yfact;
  float xwind, ywind;
  float rxwind, rywind;
  xfact = (float)source->width / dest->width;
  yfact = (float)source->height / dest->height;
  if(xfact <= 1.f) xwind = 1.f;
  else xwind = xfact;
  if(yfact <= 1.f) ywind = 1.f;
  else ywind = yfact;
  rxwind = 1.f / xwind;
  rywind = 1.f / ywind;
  if(source->has_alpha) {
    dest->has_alpha = true;
    dest->simple_alpha = false; // if it was simple before, it won't be soon
    for(int y = 0; y < dest->height; ++y) {
      Pixel*restrict d = dest->rows[y];
      int x = 0;
      float ycenter = (float)(y + 0.5f) * yfact;
      int uy = (int)(ycenter - (2.5f * ywind));
      if(uy < 0) uy = 0;
      int dy = (int)(ycenter + (3.5f * ywind));
      if(dy > source->height) dy = source->height;
      float ys = (uy - ycenter + 0.5f) * rywind;
      int rem = dest->width;
      while(rem--) {
	float rs = 0.f, gs = 0.f, bs = 0.f, as = 0.f, ts = 0.f, ta = 0.f, s;
	float xcenter = (float)(x + 0.5f) * xfact;
	int lx = (int)(xcenter - (2.5f * xwind));
	if(lx < 0) lx = 0;
	int rx = (int)(xcenter + (3.5f * xwind));
	if(rx > source->width) rx = source->width;
	float xs = (lx - xcenter + 0.5f) * rxwind;
	int sub_x, sub_y;
	float x_sinc, y_sinc;
	for(sub_y = uy, y_sinc = ys; sub_y < dy; ++sub_y, y_sinc += rywind) {
	  Pixel*restrict src = source->rows[sub_y] + lx;
	  sub_x = lx;
	  x_sinc = xs;
	  float sincy = L3(y_sinc);
	  for(; sub_x < rx; ++sub_x, x_sinc += rxwind) {
	    s = sincy * L3(x_sinc);
	    float af = ((*src >> ash) & 255) * s;
	    as += af;
	    rs += SrgbToLinear[(*src >> rsh) & 255] * af;
	    gs += SrgbToLinear[(*src >> gsh) & 255] * af;
	    bs += SrgbToLinear[(*src >> bsh) & 255] * af;
	    ++src;
	    ts += s;
	    ta += af;
	  }
	}
#ifdef __powerpc__
        __asm__("fres %0,%1" : "=f"(ts) : "f"(ts));
        __asm__("fres %0,%1" : "=f"(ta) : "f"(ta));
#else
        ts = 1.f / ts;
	ta = 1.f / ta;
#endif
	int32_t rr, rg, rb, ra;
        rr = (int32_t)(rs * ta);
        rg = (int32_t)(gs * ta);
        rb = (int32_t)(bs * ta);
	ra = (int32_t)(as * ts);
        if(rr < 0) rr = 0;
        else if(rr > 65535) rr = 65535;
        if(rg < 0) rg = 0;
        else if(rg > 65535) rg = 65535;
        if(rb < 0) rb = 0;
        else if(rb > 65535) rb = 65535;
	if(ra < 0) ra = 0;
	else if(ra > 255) ra = 255;
        *d++ = (LinearToSrgb[rr] << rsh) | (LinearToSrgb[rg] << gsh) | (LinearToSrgb[rb] << bsh) | (ra << ash);
	++x;
      }
    }
  }
  else {
    Pixel mask = 0xFF << ash;
    dest->has_alpha = false;
    for(int y = 0; y < dest->height; ++y) {
      Pixel*restrict d = dest->rows[y];
      int x = 0;
      float ycenter = (float)(y + 0.5f) * yfact;
      int uy = (int)(ycenter - (2.5f * ywind));
      if(uy < 0) uy = 0;
      int dy = (int)(ycenter + (3.5f * ywind));
      if(dy > source->height) dy = source->height;
      float ys = (uy - ycenter + 0.5f) * rywind;
      int rem = dest->width;
      while(rem--) {
	float rs = 0.f, gs = 0.f, bs = 0.f, ts = 0.f, s;
	float xcenter = (float)(x + 0.5f) * xfact;
	int lx = (int)(xcenter - (2.5f * xwind));
	if(lx < 0) lx = 0;
	int rx = (int)(xcenter + (3.5f * xwind));
	if(rx > source->width) rx = source->width;
	float xs = (lx - xcenter + 0.5f) * rxwind;
	int sub_x, sub_y;
	float x_sinc, y_sinc;
	for(sub_y = uy, y_sinc = ys; sub_y < dy; ++sub_y, y_sinc += rywind) {
	  Pixel*restrict src = source->rows[sub_y] + lx;
	  sub_x = lx;
	  x_sinc = xs;
	  float sincy = L3(y_sinc);
	  for(; sub_x < rx; ++sub_x, x_sinc += rxwind) {
	    s = sincy * L3(x_sinc);
	    rs += SrgbToLinear[(*src >> rsh) & 255] * s;
	    gs += SrgbToLinear[(*src >> gsh) & 255] * s;
	    bs += SrgbToLinear[(*src >> bsh) & 255] * s;
	    ++src;
	    ts += s;
	  }
	}
#ifdef __powerpc__
        __asm__("fres %0,%1" : "=f"(ts) : "f"(ts));
#else
        ts = 1.f / ts;
#endif
	int32_t rr, rg, rb;
        rr = (int32_t)(rs * ts);
        rg = (int32_t)(gs * ts);
        rb = (int32_t)(bs * ts);
        if(rr < 0) rr = 0;
        else if(rr > 65535) rr = 65535;
        if(rg < 0) rg = 0;
        else if(rg > 65535) rg = 65535;
        if(rb < 0) rb = 0;
        else if(rb > 65535) rb = 65535;
        *d++ = (LinearToSrgb[rr] << rsh) | (LinearToSrgb[rg] << gsh) | (LinearToSrgb[rb] << bsh) | mask;
	++x;
      }
    }
  }
  dest->Push(L);
  return 1;
}
