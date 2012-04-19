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

using namespace SubCritical;

template<int sh1, int sh2, int sh3> void BoxDown_2x2(const Drawable*restrict src, Drawable*restrict dst) {
  const Pixel*restrict srcpa, *restrict srcpb;
  Pixel*restrict dstp;
  for(int dy = 0; dy < dst->height; ++dy) {
    size_t rem = dst->width;
    unsigned long s1, s2, s3;
    srcpa = src->rows[dy*2];
    srcpb = src->rows[dy*2+1];
    dstp = dst->rows[dy];
    UNROLL(rem,
	   s1 = 0; s2 = 0; s3 = 0;
	   s1 = SrgbToLinear[(srcpa[0] >> sh1) & 255] +
	   SrgbToLinear[(srcpa[1] >> sh1) & 255] +
	   SrgbToLinear[(srcpb[0] >> sh1) & 255] +
	   SrgbToLinear[(srcpb[1] >> sh1) & 255];
	   s2 = SrgbToLinear[(srcpa[0] >> sh2) & 255] +
	   SrgbToLinear[(srcpa[1] >> sh2) & 255] +
	   SrgbToLinear[(srcpb[0] >> sh2) & 255] +
	   SrgbToLinear[(srcpb[1] >> sh2) & 255];
	   s3 = SrgbToLinear[(srcpa[0] >> sh3) & 255] +
	   SrgbToLinear[(srcpa[1] >> sh3) & 255] +
	   SrgbToLinear[(srcpb[0] >> sh3) & 255] +
	   SrgbToLinear[(srcpb[1] >> sh3) & 255];
	   srcpa += 2; srcpb += 2;
	   *dstp++ = (LinearToSrgb[(s1/4)]<<sh1)|(LinearToSrgb[(s2/4)]<<sh2)|(LinearToSrgb[(s3/4)]<<sh3););
  }
}

static void BoxDown_2x2(const Drawable*restrict src, Drawable*restrict dst) {
  switch(src->layout) {
  default:
  case FB_xRGB:
  case FB_xBGR:
    BoxDown_2x2<16,8,0>(src,dst);
    return;
  case FB_RGBx:
  case FB_BGRx:
    BoxDown_2x2<24,16,8>(src,dst);
    return;
  }
}

static void BoxDown_NxN(const Drawable*restrict src, Drawable*restrict dst, int xf, int yf) {
  int sh1, sh2, sh3;
  switch(src->layout) {
  default:
  case FB_xRGB:
  case FB_xBGR:
    sh1 = 16;
    sh2 = 8;
    sh3 = 0;
    break;
  case FB_RGBx:
  case FB_BGRx:
    sh1 = 24;
    sh2 = 16;
    sh3 = 8;
    break;
  }
  const Pixel*restrict srcp[yf];
  Pixel*restrict dstp;
  unsigned long tf = xf * yf;
  for(int dy = 0; dy < dst->height; ++dy) {
    for(int y = 0; y < yf; ++y) srcp[y] = src->rows[dy*yf+y];
    dstp = dst->rows[dy];
    for(int dx = 0; dx < dst->width; ++dx) {
      unsigned long s1 = 0, s2 = 0, s3 = 0;
      for(int y = 0; y < yf; ++y) {
	for(int x = 0; x < xf; ++x) {
	  s1 += SrgbToLinear[(*srcp[y] >> sh1) & 255];
	  s2 += SrgbToLinear[(*srcp[y] >> sh2) & 255];
	  s3 += SrgbToLinear[(*srcp[y] >> sh3) & 255];
	  ++srcp[y];
	}
      }
      *dstp++ = (LinearToSrgb[(s1/tf)]<<sh1)|(LinearToSrgb[(s2/tf)]<<sh2)|(LinearToSrgb[(s3/tf)]<<sh3);
    }
  }
}

SUBCRITICAL_UTILITY(BoxDown)(lua_State* L) {
  Drawable*restrict src = lua_toobject(L, 1, Drawable);
  Drawable*restrict dst = lua_toobject(L, 2, Drawable);
  if(src == dst)
    return luaL_error(L, "Source and destination Drawables must differ");
  if(src->layout != dst->layout) {
    if(src->IsA("Graphic"))
      ((Graphic*)src)->ChangeLayout(dst->layout);
    else if(dst->IsA("Graphic"))
      ((Graphic*)dst)->ChangeLayout(src->layout);
    else
      return luaL_error(L, "Attempt to BoxDown between two non-morphable Drawables!");
  }
  if(src->has_alpha || dst->has_alpha)
    return luaL_error(L, "You cannot use BoxDown on any Drawable with an alpha channel");
  int xf, yf;
  xf = luaL_checkinteger(L, 3);
  yf = luaL_optinteger(L, 4, xf);
  if(xf < 1)
    return luaL_error(L, "Silly X scale factor given");
  if(yf < 1)
    return luaL_error(L, "Silly Y scale factor given");
  if(xf == 1 && yf == 1)
    return luaL_error(L, "No-op BoxDown call (scale factors of 1)");
  if(dst->width * xf != src->width)
    return luaL_error(L, "Source width not equal to destination width times X scale factor");
  if(dst->height * yf != src->height)
    return luaL_error(L, "Source height not equal to destination height times Y scale factor");
  if(xf == 2 && yf == 2)
    BoxDown_2x2(src, dst);
  else
    BoxDown_NxN(src, dst, xf, yf);
  return 0;
}
