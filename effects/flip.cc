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

SUBCRITICAL_UTILITY(MirrorHorizontal)(lua_State* L) {
  Drawable*restrict old = lua_toobject(L, 1, Drawable);
  Graphic*restrict graphic = new Graphic(old->width, old->height, old->layout);
  graphic->has_alpha = old->has_alpha;
  graphic->simple_alpha = old->simple_alpha;
  for(int y = 0; y < graphic->height; ++y) {
    Pixel*restrict pl = old->rows[y], *restrict pr = graphic->rows[y] + graphic->width - 1;
    int rem = graphic->width;
    UNROLL_MORE(rem,
		*pr-- = *pl++);
  }
  graphic->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(MirrorVertical)(lua_State* L) {
  Graphic*restrict graphic = new Graphic(*lua_toobject(L, 1, Drawable));
  Pixel*restrict* pt = graphic->rows, *restrict*pb = graphic->rows + graphic->height - 1, *sw;
  int rem = graphic->height / 2;
  UNROLL_MORE(rem,
	      sw = *pt; *pt = *pb; *pb = sw;
	      ++pt; --pb;);
  graphic->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(Flip)(lua_State* L) {
  Drawable*restrict old = lua_toobject(L, 1, Drawable);
  Graphic*restrict graphic = new Graphic(old->width, old->height, old->layout);
  graphic->has_alpha = old->has_alpha;
  graphic->simple_alpha = old->simple_alpha;
  for(int y = 0; y < graphic->height; ++y) {
    Pixel*restrict pl = old->rows[y], *restrict pr = graphic->rows[y] + graphic->width - 1;
    int rem = graphic->width;
    UNROLL_MORE(rem,
		*pr-- = *pl++;);
  }
  {
    Pixel*restrict* pt = graphic->rows, *restrict*pb = graphic->rows + graphic->height - 1, *sw;
    int rem = graphic->height / 2;
    UNROLL_MORE(rem,
		sw = *pt; *pt = *pb; *pb = sw;
		++pt; --pb;);
  }
  graphic->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(RotateRight)(lua_State* L) {
  Drawable*restrict old = lua_toobject(L, 1, Drawable);
  Graphic*restrict graphic = new Graphic(old->height, old->width, old->layout);
  graphic->has_alpha = old->has_alpha;
  graphic->simple_alpha = old->simple_alpha;
  for(int y = 0; y < old->height; ++y) {
    Pixel* src = old->rows[y];
    int rem = old->width;
    int x = 0;
    UNROLL(rem,
	   graphic->rows[x++][old->height - y - 1] = *src++;);
  }
  graphic->Push(L);
  return 1;
}

SUBCRITICAL_UTILITY(RotateLeft)(lua_State* L) {
  Drawable*restrict old = lua_toobject(L, 1, Drawable);
  Graphic*restrict graphic = new Graphic(old->height, old->width, old->layout);
  graphic->has_alpha = old->has_alpha;
  graphic->simple_alpha = old->simple_alpha;
  for(int y = 0; y < old->height; ++y) {
    Pixel*restrict src = old->rows[y];
    int rem = old->width;
    int x = old->width - 1;
    UNROLL(rem,
	   graphic->rows[x--][y] = *src++;);
  }
  graphic->Push(L);
  return 1;
}
