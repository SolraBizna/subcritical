/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2013 Solra Bizna.

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
#include <stdlib.h>

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

// ScaleBest was originally repurposed from sis4, but has nothing in common
// with the old code now
#define FPI 3.141592653589793f
static inline float l3(float x) {
  if(x == 0.f) return 1.f;
  else return (sinf(FPI * x) / (FPI * x)) * (sinf(FPI * x / 3.f) / (FPI * x / 3.f));
}

#define L3SINC_BITS 9
#define L3SINC_TABLE_CENTER (1<<L3SINC_BITS)
#define L3SINC_TABLE_SIZE (L3SINC_TABLE_CENTER*2+1)
#define L3SINC_TABLE_EDGE (L3SINC_TABLE_CENTER+1)
#define L3SINC_TABLE_DIV ((L3SINC_TABLE_CENTER+1)/3.f)
static float l3sincf_table[L3SINC_TABLE_SIZE];
LUA_EXPORT int Init_effects(lua_State* L) {
  for(int y = 0; y < L3SINC_TABLE_SIZE; ++y) {
    l3sincf_table[y] = l3((y - L3SINC_TABLE_CENTER) / L3SINC_TABLE_DIV);
  }
  return 0;
}
#define L3(x) l3sincf_table[(int)(x * L3SINC_TABLE_DIV + L3SINC_TABLE_CENTER)];
/*static float L3(float x) {
  int y = (int)(x * L3SINC_TABLE_DIV + L3SINC_TABLE_CENTER);
  if(y >= L3SINC_TABLE_SIZE) {
    fprintf(stderr, "L3 out of range (too high)\n");
    abort();
  }
  else if(y < 0) {
    fprintf(stderr, "L3 out of range (too low)\n");
    abort();
  }
  return l3sincf_table[y];
  }*/

static inline float FastFloat(uint32_t u) {
  union {
    float f;
    uint32_t u;
  } x;
  x.u = 0x4B000000U + u;
  return x.f - 8388608.f;
}

struct LanczosKernel {
  int32_t start;
  uint32_t rem, count;
  float* kernel, rtotal;
  LanczosKernel() : start(0), rem(0), count(0), kernel(NULL) {}
  LanczosKernel(int32_t dst, float fact, float inc, int32_t size) : kernel(NULL) {
    Setup(dst, fact, inc, size);
  }
  void Setup(int32_t dst, float fact, float inc, int32_t size) {
    float left, right;
    int32_t stop;
    if(fact > 1.f) {
      left = (dst - 3.f) * fact - 0.5f;
      right = (dst + 3.f) * fact - 0.5f;
    }
    else {
      left = dst * fact - 3.5f;
      right = dst * fact + 2.5f;
    }
    float fstart = left + 1.f;
    if(fstart < 0.f) fstart = 0.f;
    else fstart = floorf(fstart);
    start = (int32_t)fstart;
    stop = (int32_t)ceilf(right)-1;
    if(stop >= size) stop = size - 1;
    rem = (stop - start) + 1;
    if(rem > count) {
      count = rem;
      kernel = (float*)realloc(kernel, sizeof(float)*count);
      if(!kernel) abort(); // :|
    }
    float x = -3.f + inc * (fstart - left);
    uint32_t rem = this->rem;
    float* p = kernel;
    float l;
    rtotal = 0.f;
    UNROLL(rem,
           l = L3(x);
           *p++ = l;
           rtotal += l;
           x += inc;
           if(x >= 3.f) goto owari;);
    /* shamefully, this is what I did instead of making sure I got the math
       right */
  owari:
    rtotal = 1.f / rtotal;
    this->rem = p - kernel;
  }
  ~LanczosKernel() {
    if(kernel) {
      free(kernel);
      kernel = NULL;
    }
  }
};

SUBCRITICAL_UTILITY(ScaleBest)(lua_State* L) {
  Drawable*restrict source = lua_toobject(L, 1, Drawable);
  Drawable*restrict dest;
  int callback;
  int rowskip = 0, rowskiprem = 0;
  if(lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TUSERDATA) {
    dest = lua_toobject(L, 2, Drawable);
    if(dest->layout != source->layout) {
      if(source->IsA("Graphic"))
        ((Graphic*)source)->ChangeLayout(dest->layout);
      else if(dest->IsA("Graphic"))
        ((Graphic*)dest)->ChangeLayout(source->layout);
      else
        return luaL_error(L, "Attempt to ScaleBest between two non-morphable Drawables!");
    }
    callback = lua_gettop(L) >= 3 ? 3 : 0;
    lua_pushvalue(L, 2);
  }
  else {
    dest = new Graphic(luaL_checkinteger(L, 2), luaL_checkinteger(L, 3), source->layout);
    callback = lua_gettop(L) >= 4 ? 4 : 0;
    dest->Push(L);
  }
  if(source == dest) return luaL_error(L, "source and destination must differ");
  if(callback) {
    // yes callback!
    rowskip = luaL_optinteger(L, callback + 1, 1);
    if(rowskip < 1) rowskip = 1;
    rowskiprem = rowskip;
  }
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
  xfact = (float)source->width / dest->width;
  yfact = (float)source->height / dest->height;
  float xinc, yinc;
  if(xfact > 1.f) xinc = 1.f / xfact;
  else xinc = 1.f;
  if(yfact > 1.f) yinc = 1.f / yfact;
  else yinc = 1.f;
  /* the X kernels will be the same for each row */
  LanczosKernel x_kernels[dest->width];
  for(int32_t x = 0; x < dest->width; ++x) {
    x_kernels[x].Setup(x, xfact, xinc, source->width);
  }
  // the Y kernel is constant across a given row
  LanczosKernel y_kernel;
  if(source->has_alpha) {
    /* the alpha code is a slight modification of the below */
    dest->has_alpha = true;
    dest->simple_alpha = false; // if it was simple before, it won't be soon
    float interbuf[source->width*4];
    dest->has_alpha = false;
    int source_pitch = (const uint8_t*)source->rows[1] - (const uint8_t*)source->rows[0];
    for(int y = 0; y < dest->height; ++y) {
      float* fp = interbuf;
      y_kernel.Setup(y, yfact, yinc, source->height);
      for(int x = 0; x < source->width; ++x) {
        float tr = 0.f, tg = 0.f, tb = 0.f, ta = 0.f;
        float a;
        const Pixel* srcp = source->rows[y_kernel.start] + x;
        uint32_t rem = y_kernel.rem;
        float* lp = y_kernel.kernel;
        UNROLL_MORE(rem,
                    a = FastFloat((*srcp >> ash) & 255) * *lp;
                    ta += a;
                    tr += FastFloat(SrgbToLinear[(*srcp >> rsh) & 255]) * a;
                    tg += FastFloat(SrgbToLinear[(*srcp >> gsh) & 255]) * a;
                    tb += FastFloat(SrgbToLinear[(*srcp >> bsh) & 255]) * a;
                    ++lp;
                    srcp = (const Pixel*)((const uint8_t*)srcp + source_pitch););
        *fp++ = ta * y_kernel.rtotal;
        if(ta != 0.f) {
#ifdef __powerpc__
          __asm__("fres %0,%1" : "=f"(ta) : "f"(ta));
#else
          ta = 1.f / ta;
#endif
        }
        *fp++ = tr * ta;
        *fp++ = tg * ta;
        *fp++ = tb * ta;
      }
      Pixel*restrict dp = dest->rows[y];
      for(int x = 0; x < dest->width; ++x) {
        LanczosKernel& x_kernel = x_kernels[x];
        float tr = 0.f, tg = 0.f, tb = 0.f, ta = 0.f;
        uint32_t rem = x_kernel.rem;
        float* lp = x_kernel.kernel;
        float a;
        fp = interbuf + x_kernel.start * 4;
        UNROLL_MORE(rem,
                    a = *fp++ * *lp;
                    ta += a;
                    tr += *fp++ * a;
                    tg += *fp++ * a;
                    tb += *fp++ * a;
                    ++lp;);
        int32_t ia;
        ia = (int32_t)(ta * x_kernel.rtotal);
        if(ta != 0.f) {
#ifdef __powerpc__
          __asm__("fres %0,%1" : "=f"(ta) : "f"(ta));
#else
          ta = 1.f / ta;
#endif
        }
        if(ia < 0) ia = 0;
        else if(ia > 255) ia = 255;
        int32_t ir, ig, ib;
        ir = (int32_t)(tr * ta);
        ig = (int32_t)(tg * ta);
        ib = (int32_t)(tb * ta);
        if(ir < 0) ir = 0;
        else if(ir > 65535) ir = 65535;
        if(ig < 0) ig = 0;
        else if(ig > 65535) ig = 65535;
        if(ib < 0) ib = 0;
        else if(ib > 65535) ib = 65535;
        *dp++ = (LinearToSrgb[ir] << rsh) | (LinearToSrgb[ig] << gsh) | (LinearToSrgb[ib] << bsh) | (ia << ash);
      }
    }
  }
  else {
    Pixel mask = 0xFF << ash;
    float interbuf[source->width*3];
    dest->has_alpha = false;
    int source_pitch = (const uint8_t*)source->rows[1] - (const uint8_t*)source->rows[0];
    for(int y = 0; y < dest->height; ++y) {
      float* fp = interbuf;
      y_kernel.Setup(y, yfact, yinc, source->height);
      for(int x = 0; x < source->width; ++x) {
        float tr = 0.f, tg = 0.f, tb = 0.f;
        const Pixel* srcp = source->rows[y_kernel.start] + x;
        uint32_t rem = y_kernel.rem;
        float* lp = y_kernel.kernel;
        UNROLL_MORE(rem,
                    tr += FastFloat(SrgbToLinear[(*srcp >> rsh) & 255]) * *lp;
                    tg += FastFloat(SrgbToLinear[(*srcp >> gsh) & 255]) * *lp;
                    tb += FastFloat(SrgbToLinear[(*srcp >> bsh) & 255]) * *lp;
                    ++lp;
                    srcp = (const Pixel*)((const uint8_t*)srcp + source_pitch););
        *fp++ = tr * y_kernel.rtotal;
        *fp++ = tg * y_kernel.rtotal;
        *fp++ = tb * y_kernel.rtotal;
      }
      Pixel*restrict dp = dest->rows[y];
      for(int x = 0; x < dest->width; ++x) {
        LanczosKernel& x_kernel = x_kernels[x];
        float tr = 0.f, tg = 0.f, tb = 0.f;
        uint32_t rem = x_kernel.rem;
        float* lp = x_kernel.kernel;
        fp = interbuf + x_kernel.start * 3;
        UNROLL_MORE(rem,
                    tr += *fp++ * *lp;
                    tg += *fp++ * *lp;
                    tb += *fp++ * *lp;
                    ++lp;);
        int32_t ir, ig, ib;
        ir = (int32_t)(tr * x_kernel.rtotal);
        ig = (int32_t)(tg * x_kernel.rtotal);
        ib = (int32_t)(tb * x_kernel.rtotal);
        if(ir < 0) ir = 0;
        else if(ir > 65535) ir = 65535;
        if(ig < 0) ig = 0;
        else if(ig > 65535) ig = 65535;
        if(ib < 0) ib = 0;
        else if(ib > 65535) ib = 65535;
        *dp++ = (LinearToSrgb[ir] << rsh) | (LinearToSrgb[ig] << gsh) | (LinearToSrgb[ib] << bsh) | mask;
      }
      if(callback && --rowskiprem == 0) {
        rowskiprem = rowskip;
        // callback
        lua_pushvalue(L, callback);
        // callback, destination
        lua_pushvalue(L, -2);
        // callback, destination, row
        lua_pushinteger(L, y);
        // result
        lua_call(L, 2, 1);
        bool abort = lua_toboolean(L, -1);
        lua_pop(L, 1);
        if(abort) break;
      }
    }
  }
  return 1;
}
