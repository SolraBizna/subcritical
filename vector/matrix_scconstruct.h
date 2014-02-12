// -*- c++ -*-
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

C4(Mat,R,x,C)::C4(Mat,R,x,C)(Scalar entries[R*C]) : Matrix(R,C) {
  int n = 0;
  xx = entries[n++];
  yx = entries[n++];
#if R > 2
  zx = entries[n++];
#if R > 3
  wx = entries[n++];
#endif
#endif
  xy = entries[n++];
  yy = entries[n++];
#if R > 2
  zy = entries[n++];
#if R > 3
  wy = entries[n++];
#endif
#endif
#if C > 2
  xz = entries[n++];
  yz = entries[n++];
#if R > 2
  zz = entries[n++];
#if R > 3
  wz = entries[n++];
#endif
#endif
#if C > 3
  xw = entries[n++];
  yw = entries[n++];
#if R > 2
  zw = entries[n++];
#if R > 3
  ww = entries[n++];
#endif
#endif
#endif
#endif
}

C4(Mat,R,x,C)::C4(Mat,R,x,C)() : Matrix(R,C),
xx(1), yx(0)
#if R > 2
  ,zx(0)
#if R > 3
  ,wx(0)
#endif
#endif
  ,xy(0),yy(1)
#if R > 2
  ,zy(0)
#if R > 3
  ,wy(0)
#endif
#endif
#if C > 2
  ,xz(0), yz(0)
#if R > 2
  ,zz(1)
#if R > 3
  ,wz(0)
#endif
#endif
#if C > 3
  ,xw(0), yw(0)
#if R > 2
  ,zw(0)
#if R > 3
  ,ww(1)
#endif
#endif
#endif
#endif
{}

LUA_EXPORT int C4(Construct_Mat,R,x,C)(lua_State* L) {
  if(lua_gettop(L) == 0) {
    (new C4(Mat,R,x,C)())->Push(L);
    return 1;
  }
  Scalar entries[R*C];
  for(int n = 0; n < R*C; ++n) {
    entries[n] = luaL_checknumber(L, n+1);
  }
  (new C4(Mat,R,x,C)(entries))->Push(L);
  return 1;
}
