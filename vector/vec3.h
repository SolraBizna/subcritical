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

int Vec3::Normalize(lua_State* L) restrict {
  Scalar magnitude = x*x + y*y + z*z;
  Vec3*restrict ret;
  if(magnitude != 0) {
    magnitude = 1.0 / sqrt(magnitude);
    ret = new Vec3(x*magnitude, y*magnitude, z*magnitude);
  }
  else ret = new Vec3(x, y, z);
  ret->Push(L);
  return 1;
}

int Vec3::Magnitude(lua_State* L) {
  Scalar magnitude = x*x + y*y + z*z;
  if(magnitude != 0) lua_pushnumber(L, sqrt(magnitude));
  else lua_pushnumber(L, 0);
  return 1;
}

int Vec3::AngleXY(lua_State* L) {
  Scalar angle = atan2(y,x);
  lua_pushnumber(L, angle);
  return 1;
}

int Vec3::AngleXZ(lua_State* L) {
  Scalar angle = atan2(z,x);
  lua_pushnumber(L, angle);
  return 1;
}

int Vec3::AngleYZ(lua_State* L) {
  Scalar angle = atan2(z,y);
  lua_pushnumber(L, angle);
  return 1;
}

int Vec3::MagnitudeSquared(lua_State* L) {
  Scalar magnitude = x*x + y*y + z*z;
  lua_pushnumber(L, magnitude);
  return 1;
}

static Vec3*restrict vec_mul_3(const Vec3& a, Scalar b) {
  Vec3*restrict ret = new Vec3();
  ret->x = a.x * b;
  ret->y = a.y * b;
  ret->z = a.z * b;
  return ret;
}

static Vec3*restrict vec_div_3(const Vec3& a, Scalar b) {
  Vec3*restrict ret = new Vec3();
  Scalar recip= 1 / b;
  ret->x = a.x * recip;
  ret->y = a.y * recip;
  ret->z = a.z * recip;
  return ret;
}

static Vec3*restrict vec_sadd_3(const Vec3& a, Scalar b) {
  Vec3*restrict ret = new Vec3();
  ret->x = a.x + b;
  ret->y = a.y + b;
  ret->z = a.z + b;
  return ret;
}

static Vec3*restrict vec_ssub_3(const Vec3& a, Scalar b) {
  Vec3*restrict ret = new Vec3();
  ret->x = a.x - b;
  ret->y = a.y - b;
  ret->z = a.z - b;
  return ret;
}

static Vec3*restrict vec_mul_333(const Vec3&restrict a, const Vec3&restrict b) {
  Vec3*restrict ret = new Vec3();
  ret->x = a.x * b.x;
  ret->y = a.y * b.y;
  ret->z = a.z * b.z;
  return ret;
}

static Vec3*restrict vec_unm_3(const Vec3&restrict a) {
  Vec3*restrict ret = new Vec3();
  ret->x = -a.x;
  ret->y = -a.y;
  ret->z = -a.z;
  return ret;
}

static bool vec_eq_3(const Vec3&restrict a, const Vec3&restrict b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

static int vec_unpack_3(lua_State* L, const Vec3& a) {
  lua_pushnumber(L, a.x);
  lua_pushnumber(L, a.y);
  lua_pushnumber(L, a.z);
  return 3;
}

static Scalar vec_index_3(lua_State* L, const Vec3& a, int n) {
  switch(n) {
  case 1: return a.x;
  case 2: return a.y;
  case 3: return a.z;
  case 4: return 1;
  default: return luaL_error(L, "this code should not be reached!");
  }
}

static int vec_newindex_3(lua_State* L, Vec3& a, int n, Scalar v) {
  switch(n) {
  case 1: a.x = v; break;
  case 2: a.y = v; break;
  case 3: a.z = v; break;
  case 4: return luaL_error(L, "attempt to set element %d of a 3-element vector", n);
  default: return luaL_error(L, "this code should not be reached!");
  }
  return 0;
}

#define A Vec3
#define B Vec3
#define an 3
#define bn 3
#include "sub_vec3.h"
#undef A
#undef B
#undef an
#undef bn

#define A Vec3
#define B Vec2
#define an 3
#define bn 2
#include "sub_vec3.h"
#undef A
#undef B
#undef an
#undef bn

#define A Vec2
#define B Vec3
#define an 2
#define bn 3
#include "sub_vec3.h"
#undef A
#undef B
#undef an
#undef bn
