// -*- c++ -*-
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

#define SPLAT_C "mat_vec.h"
#include "splat_c.h"
#undef SPLAT_C

static VectorArray*restrict C4(mat_mul_,A,B,_VA)(const LEFT&restrict a, const VectorArray*restrict b) {
  VectorArray*restrict ret = new VectorArray(b->order, b->count);
  switch(b->order) {
  case 2:
    for(uint32_t n = 0; n < b->count*2; n += 2) {
      C4(mat_mul_,A,B,_D2)(a, b->buffer+n, ret->buffer+n);
    }
    break;
  case 3:
    for(uint32_t n = 0; n < b->count*3; n += 3) {
      C4(mat_mul_,A,B,_D3)(a, b->buffer+n, ret->buffer+n);
    }
    break;
  case 4:
    for(uint32_t n = 0; n < b->count*4; n += 4) {
      C4(mat_mul_,A,B,_D4)(a, b->buffer+n, ret->buffer+n);
    }
    break;
  default:
    fprintf(stderr, "This is so weird it isn't even worth attempting to recover from.\n");
    throw 3.1415926535897932384626;
  }
  return ret;
}

LOCAL int C4(Mat,A,x,B)::MultiplyAndCompile(lua_State* L) {
  VectorArray* b = lua_toobject(L, 1, VectorArray);
  Fixed dx, dy;
  dx = F_TO_Q(luaL_optnumber(L, 2, 0));
  dy = F_TO_Q(luaL_optnumber(L, 3, 0));
  CoordArray* ret = new CoordArray(b->count);
  lua_settop(L, 1);
  ret->Push(L);
  Scalar buffer[4];
  Fixed* out = ret->coords;
  Scalar* in = b->buffer;
  for(uint32_t n = 0; n < b->count; ++n) {
    switch(b->order) {
    case 2: C4(mat_mul_,A,B,_D2)(*this, in, buffer); break;
    case 3: C4(mat_mul_,A,B,_D3)(*this, in, buffer); break;
    case 4: C4(mat_mul_,A,B,_D4)(*this, in, buffer); break;
    default:
      fprintf(stderr, "This is so weird it isn't even worth attempting to recover from.\n");
      throw 3.1415926535897932384626;
    }
    *out++ = F_TO_Q(buffer[0]) + dx;
    *out++ = F_TO_Q(buffer[1]) + dy;
    in += b->order;
  }
  return 1;
}

#if A >= 3
LOCAL int C4(Mat,A,x,B)::PerspectiveMultiplyAndCompile(lua_State* L) {
  VectorArray* b = lua_toobject(L, 1, VectorArray);
  Fixed dx, dy;
  dx = F_TO_Q(luaL_optnumber(L, 2, 0));
  dy = F_TO_Q(luaL_optnumber(L, 3, 0));
  CoordArray* ret = new CoordArray(b->count);
  lua_settop(L, 1);
  ret->Push(L);
  Scalar buffer[4];
  Fixed* out = ret->coords;
  Scalar* in = b->buffer;
  for(uint32_t n = 0; n < b->count; ++n) {
    switch(b->order) {
    case 2: C4(mat_mul_,A,B,_D2)(*this, in, buffer); break;
    case 3: C4(mat_mul_,A,B,_D3)(*this, in, buffer); break;
    case 4: C4(mat_mul_,A,B,_D4)(*this, in, buffer); break;
    default:
      fprintf(stderr, "This is so weird it isn't even worth attempting to recover from.\n");
      throw 3.1415926535897932384626;
    }
    Scalar rz;
    if(buffer[2] == 0 || (rz = 1 / buffer[2]) == 0) {
      *out++ = (buffer[0] > 0 ? 16777216 : -16777216) + dx;
      *out++ = (buffer[1] > 0 ? 16777216 : -16777216) + dy;
    }
    else {
      *out++ = F_TO_Q(buffer[0] * rz) + dx;
      *out++ = F_TO_Q(buffer[1] * rz) + dy;
    }
    in += b->order;
  }
  return 1;
}
#endif
