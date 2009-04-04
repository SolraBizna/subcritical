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

int Vec4::Normalize(lua_State* L) {
  Scalar magnitude = x*x + y*y + z*z + w*w;
  Vec4* ret;
  if(magnitude != 0) {
    magnitude = 1.0 / sqrt(magnitude);
    ret = new Vec4(x*magnitude, y*magnitude, z*magnitude, w*magnitude);
  }
  else ret = new Vec4(x, y, z, w);
  ret->Push(L);
  return 1;
}

int Vec4::Magnitude(lua_State* L) {
  Scalar magnitude = x*x + y*y + z*z + w*w;
  if(magnitude != 0) lua_pushnumber(L, sqrt(magnitude));
  else lua_pushnumber(L, 0);
  return 1;
}

int Vec4::MagnitudeSquared(lua_State* L) {
  Scalar magnitude = x*x + y*y + z*z + w*w;
  lua_pushnumber(L, magnitude);
  return 1;
}

static Vec4* vec_mul_4(const Vec4& a, Scalar b) {
  Vec4* ret = new Vec4();
  ret->x = a.x * b;
  ret->y = a.y * b;
  ret->z = a.z * b;
  ret->w = a.w * b;
  return ret;
}

static Vec4* vec_unm_4(const Vec4& a) {
  Vec4* ret = new Vec4();
  ret->x = -a.x;
  ret->y = -a.y;
  ret->z = -a.z;
  ret->w = -a.w;
  return ret;
}

static bool vec_eq_4(const Vec4& a, const Vec4& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static int vec_unpack_4(lua_State* L, const Vec4& a) {
  lua_pushnumber(L, a.x);
  lua_pushnumber(L, a.y);
  lua_pushnumber(L, a.z);
  lua_pushnumber(L, a.w);
  return 4;
}

static Scalar vec_index_4(lua_State* L, const Vec4& a, int n) {
  switch(n) {
  case 1: return a.x;
  case 2: return a.y;
  case 3: return a.z;
  case 4: return a.w;
  default: return luaL_error(L, "this code should not be reached!");
  }
}

static int vec_newindex_4(lua_State* L, Vec4& a, int n, Scalar v) {
  switch(n) {
  case 1: a.x = v; break;
  case 2: a.y = v; break;
  case 3: a.z = v; break;
  case 4: a.w = v; break;
  default: return luaL_error(L, "this code should not be reached!");
  }
  return 0;
}


#define A Vec4
#define B Vec4
#define an 4
#define bn 4
#include "sub_vec4.h"
#undef A
#undef B
#undef an
#undef bn

#define A Vec4
#define B Vec3
#define an 4
#define bn 3
#include "sub_vec4.h"
#undef A
#undef B
#undef an
#undef bn

#define A Vec4
#define B Vec2
#define an 4
#define bn 2
#include "sub_vec4.h"
#undef A
#undef B
#undef an
#undef bn

#define A Vec3
#define B Vec4
#define an 3
#define bn 4
#include "sub_vec4.h"
#undef A
#undef B
#undef an
#undef bn

#define A Vec2
#define B Vec4
#define an 2
#define bn 4
#include "sub_vec4.h"
#undef A
#undef B
#undef an
#undef bn

