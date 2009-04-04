// -*- c++ -*-
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

#if A > C
#define E A
#else
#define E C
#endif
#if B > D
#define F B
#else
#define F D
#endif

#define PRIME C4(Mat,E,x,F)
#define LEFT C4(Mat,A,x,B)

static int C4(mat_unpack_,A,x,B)(lua_State* L, const LEFT& a) {
  lua_checkstack(L, A * B);
  lua_pushnumber(L, a.xx);
  lua_pushnumber(L, a.yx);
#if A >= 3
  lua_pushnumber(L, a.zx);
#if A >= 4
  lua_pushnumber(L, a.wx);
#endif
#endif
  lua_pushnumber(L, a.xy);
  lua_pushnumber(L, a.yy);
#if A >= 3
  lua_pushnumber(L, a.zy);
#if A >= 4
  lua_pushnumber(L, a.wy);
#endif
#endif
#if B >= 3
  lua_pushnumber(L, a.xz);
  lua_pushnumber(L, a.yz);
#if A >= 3
  lua_pushnumber(L, a.zz);
#if A >= 4
  lua_pushnumber(L, a.wz);
#endif
#endif
#if B >= 4
  lua_pushnumber(L, a.xw);
  lua_pushnumber(L, a.yw);
#if A >= 3
  lua_pushnumber(L, a.zw);
#if A >= 4
  lua_pushnumber(L, a.ww);
#endif
#endif
#endif
#endif
  return A*B;
}

#define SPLAT_C "splat_d.h"
#define SPLAT_D "mat_double.h"
#include "splat_c.h"
#undef SPLAT_C
#undef SPLAT_D

#undef E
#undef F
#undef PRIME

#define SPLAT_C "mat_vec.h"
#include "splat_c.h"
#undef SPLAT_C

