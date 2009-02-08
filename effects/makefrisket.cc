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

using namespace SubCritical;

SUBCRITICAL_UTILITY(MakeFrisketFromAlpha)(lua_State* L) throw() {
  Drawable* graphic = lua_toobject(L, 1, Drawable);
  if(!graphic->has_alpha) return luaL_error(L, "MakeFrisketFromAlpha called on graphic with no alpha channel!");
  Frisket* ret = new Frisket(graphic->width, graphic->height);
  int ash;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Making frisket from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
  case FB_xBGR:
    ash = 24; break;
  case FB_RGBx:
  case FB_BGRx:
    ash = 0; break;
  }
  Pixel** srcp;
  Frixel** dstp;
  for(srcp = graphic->rows, dstp = ret->rows; srcp < graphic->rows + graphic->height; ++srcp, ++dstp) {
    Pixel* src = *srcp;
    Frixel* dst = *dstp;
    size_t rem = graphic->width;
    UNROLL(rem,
	   *dst++ = *src++ >> ash);
  }
  ret->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(MakeFrisketFromGrayscale)(lua_State* L) throw() {
  Drawable* graphic = lua_toobject(L, 1, Drawable);
  Frisket* ret = new Frisket(graphic->width, graphic->height);
  int rsh, gsh, bsh;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Making frisket from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
    rsh = 16;
    gsh = 8;
    bsh = 0;
    break;
  case FB_xBGR:
    rsh = 0;
    gsh = 8;
    bsh = 16;
    break;
  case FB_RGBx:
    rsh = 24;
    gsh = 16;
    bsh = 8;
    break;
  case FB_BGRx:
    rsh = 8;
    gsh = 16;
    bsh = 24;
    break;
  }
  Pixel** srcp;
  Frixel** dstp;
  for(srcp = graphic->rows, dstp = ret->rows; srcp < graphic->rows + graphic->height; ++srcp, ++dstp) {
    Pixel* src = *srcp;
    Frixel* dst = *dstp;
    size_t rem = graphic->width;
    UNROLL(rem,
	   *dst++ = ((SrgbToLinear[(*src >> rsh) & 255] * 299 + SrgbToLinear[(*src >> gsh) & 255] * 587 + SrgbToLinear[(*src >> bsh) & 255] * 114) / 1000) >> 8;
	   ++src);
  }
  ret->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(MakeFrisketFromGrayscaleQuickly)(lua_State* L) throw() {
  Drawable* graphic = lua_toobject(L, 1, Drawable);
  Frisket* ret = new Frisket(graphic->width, graphic->height);
  int rsh, gsh, bsh;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Making frisket from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
    rsh = 16;
    gsh = 8;
    bsh = 0;
    break;
  case FB_xBGR:
    rsh = 0;
    gsh = 8;
    bsh = 16;
    break;
  case FB_RGBx:
    rsh = 24;
    gsh = 16;
    bsh = 8;
    break;
  case FB_BGRx:
    rsh = 8;
    gsh = 16;
    bsh = 24;
    break;
  }
  Pixel** srcp;
  Frixel** dstp;
  for(srcp = graphic->rows, dstp = ret->rows; srcp < graphic->rows + graphic->height; ++srcp, ++dstp) {
    Pixel* src = *srcp;
    Frixel* dst = *dstp;
    size_t rem = graphic->width;
    UNROLL(rem,
	   *dst++ = ((SrgbToLinear[(*src >> rsh) & 255] + SrgbToLinear[(*src >> gsh) & 255] + SrgbToLinear[(*src >> bsh) & 255]) / 3) >> 8;
	   ++src);
  }
  ret->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(MakeFrisketFromRed)(lua_State* L) throw() {
  Drawable* graphic = lua_toobject(L, 1, Drawable);
  Frisket* ret = new Frisket(graphic->width, graphic->height);
  int rsh;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Making frisket from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
    rsh = 16;
    break;
  case FB_xBGR:
    rsh = 0;
    break;
  case FB_RGBx:
    rsh = 24;
    break;
  case FB_BGRx:
    rsh = 8;
    break;
  }
  Pixel** srcp;
  Frixel** dstp;
  for(srcp = graphic->rows, dstp = ret->rows; srcp < graphic->rows + graphic->height; ++srcp, ++dstp) {
    Pixel* src = *srcp;
    Frixel* dst = *dstp;
    size_t rem = graphic->width;
    UNROLL(rem,
	   *dst++ = SrgbToLinear[(*src++ >> rsh) & 255] >> 8);
  }
  ret->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(MakeFrisketFromGreen)(lua_State* L) throw() {
  Drawable* graphic = lua_toobject(L, 1, Drawable);
  Frisket* ret = new Frisket(graphic->width, graphic->height);
  int gsh;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Making frisket from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
  case FB_xBGR:
    gsh = 8;
    break;
  case FB_BGRx:
  case FB_RGBx:
    gsh = 16;
    break;
  }
  Pixel** srcp;
  Frixel** dstp;
  for(srcp = graphic->rows, dstp = ret->rows; srcp < graphic->rows + graphic->height; ++srcp, ++dstp) {
    Pixel* src = *srcp;
    Frixel* dst = *dstp;
    size_t rem = graphic->width;
    UNROLL(rem,
	   *dst++ = SrgbToLinear[(*src++ >> gsh) & 255] >> 8);
  }
  ret->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(MakeFrisketFromBlue)(lua_State* L) throw() {
  Drawable* graphic = lua_toobject(L, 1, Drawable);
  Frisket* ret = new Frisket(graphic->width, graphic->height);
  int bsh;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Making frisket from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
    bsh = 0;
    break;
  case FB_xBGR:
    bsh = 16;
    break;
  case FB_RGBx:
    bsh = 8;
    break;
  case FB_BGRx:
    bsh = 24;
    break;
  }
  Pixel** srcp;
  Frixel** dstp;
  for(srcp = graphic->rows, dstp = ret->rows; srcp < graphic->rows + graphic->height; ++srcp, ++dstp) {
    Pixel* src = *srcp;
    Frixel* dst = *dstp;
    size_t rem = graphic->width;
    UNROLL(rem,
	   *dst++ = SrgbToLinear[(*src++ >> bsh) & 255] >> 8);
  }
  ret->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(MakeFrisketDirectly)(lua_State* L) throw() {
  Drawable* graphic = lua_toobject(L, 1, Drawable);
  Frisket* ret = new Frisket(graphic->width, graphic->height);
  int gsh;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Making frisket from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
  case FB_xBGR:
    gsh = 8;
    break;
  case FB_RGBx:
  case FB_BGRx:
    gsh = 16;
    break;
  }
  Pixel** srcp;
  Frixel** dstp;
  for(srcp = graphic->rows, dstp = ret->rows; srcp < graphic->rows + graphic->height; ++srcp, ++dstp) {
    Pixel* src = *srcp;
    Frixel* dst = *dstp;
    size_t rem = graphic->width;
    UNROLL(rem,
	   *dst++ = *src >> gsh;
	   ++src);
  }
  ret->Push(L);
  return 1;
}
