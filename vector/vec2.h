// -*- c++ -*-
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

int Vec2::Normalize(lua_State* L) restrict {
  Scalar magnitude = x*x + y*y;
  Vec2*restrict ret;
  if(magnitude != 0) {
    magnitude = 1.0 / sqrt(magnitude);
    ret = new Vec2(x*magnitude, y*magnitude);
  }
  else ret = new Vec2(x, y);
  ret->Push(L);
  return 1;
}

int Vec2::Magnitude(lua_State* L) {
  Scalar magnitude = x*x + y*y;
  if(magnitude != 0) lua_pushnumber(L, sqrt(magnitude));
  else lua_pushnumber(L, 0);
  return 1;
}

int Vec2::Angle(lua_State* L) {
  Scalar angle = atan2(y,x);
  lua_pushnumber(L, angle);
  return 1;
}

int Vec2::MagnitudeSquared(lua_State* L) {
  Scalar magnitude = x*x + y*y;
  lua_pushnumber(L, magnitude);
  return 1;
}

static Vec2*restrict vec_mul_2(const Vec2&restrict a, Scalar b) {
  Vec2*restrict ret = new Vec2();
  ret->x = a.x * b;
  ret->y = a.y * b;
  return ret;
}

static Vec2*restrict vec_div_2(const Vec2&restrict a, Scalar b) {
  Vec2*restrict ret = new Vec2();
  Scalar recip= 1 / b;
  ret->x = a.x * recip;
  ret->y = a.y * recip;
  return ret;
}

static Vec2*restrict vec_sadd_2(const Vec2&restrict a, Scalar b) {
  Vec2*restrict ret = new Vec2();
  ret->x = a.x + b;
  ret->y = a.y + b;
  return ret;
}

static Vec2*restrict vec_ssub_2(const Vec2&restrict a, Scalar b) {
  Vec2*restrict ret = new Vec2();
  ret->x = a.x - b;
  ret->y = a.y - b;
  return ret;
}


static Vec2*restrict vec_unm_2(const Vec2&restrict a) {
  Vec2*restrict ret = new Vec2();
  ret->x = -a.x;
  ret->y = -a.y;
  return ret;
}

static Vec2*restrict vec_add_222(const Vec2&restrict a, const Vec2&restrict b) {
  Vec2*restrict ret = new Vec2();
  ret->x = a.x + b.x;
  ret->y = a.y + b.y;
  return ret;
}

static Vec2*restrict vec_sub_222(const Vec2&restrict a, const Vec2&restrict b) {
  Vec2*restrict ret = new Vec2();
  ret->x = a.x - b.x;
  ret->y = a.y - b.y;
  return ret;
}

static Vec2*restrict vec_mul_222(const Vec2&restrict a, const Vec2&restrict b) {
  Vec2*restrict ret = new Vec2();
  ret->x = a.x * b.x;
  ret->y = a.y * b.y;
  return ret;
}

static Scalar vec_concat_222(const Vec2&restrict a, const Vec2&restrict b) {
  return a.x*b.x + a.y*b.y;
}

static bool vec_eq_2(const Vec2&restrict a, const Vec2&restrict b) {
  return a.x == b.x && a.y == b.y;
}

static int vec_unpack_2(lua_State* L, const Vec2&restrict a) {
  lua_pushnumber(L, a.x);
  lua_pushnumber(L, a.y);
  return 2;
}

static Scalar vec_index_2(lua_State* L, const Vec2&restrict a, int n) {
  switch(n) {
  case 1: return a.x;
  case 2: return a.y;
  case 3: return 0;
  case 4: return 1;
  default: return luaL_error(L, "this code should not be reached!");
  }
}

static int vec_newindex_2(lua_State* L, Vec2&restrict a, int n, Scalar v) {
  switch(n) {
  case 1: a.x = v; break;
  case 2: a.y = v; break;
  case 3:
  case 4: return luaL_error(L, "attempt to set element %d of a 2-element vector", n);
  default: return luaL_error(L, "this code should not be reached!");
  }
  return 0;
}
