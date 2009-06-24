// -*- c++ -*-
/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2009 Solra Bizna.

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

SUBCRITICAL_CONSTRUCTOR(VectorArray)(lua_State* L) {
  if(!lua_istable(L, 1)) return luaL_typerror(L, 1, "table of Vectors");
  uint32_t count = lua_objlen(L, 1);
  lua_rawgeti(L,1,1);
  Vector* v = lua_toobject(L, -1, Vector);
  uint32_t order = v->n;
  lua_settop(L,1);
  VectorArray* ret = new VectorArray(order, count);
  ret->Push(L);
  for(uint32_t i = 1; i <= count; ++i) {
    lua_rawgeti(L, 1, i);
    // all Vectors are at least Vec2s
    Vec2* vec = (Vec2*)lua_toobject(L, -1, Vector);
    if((uint32_t)vec->n != order) return luaL_error(L, "all Vectors in the array must be of the same order");
    int n = i - 1;
    ret->buffer[n*order] = vec->x;
    ret->buffer[n*order+1] = vec->y;
    if(order > 2) {
      ret->buffer[n*order+2] = ((Vec3*)vec)->z;
      if(order > 3) {
	ret->buffer[n*order+3] = ((Vec4*)vec)->w;
      }
    }
    lua_settop(L, 2);
  }
  return 1;
}

SUBCRITICAL_UTILITY(CompileVectors)(lua_State* L) {
  if(!lua_istable(L, 1)) return luaL_typerror(L, 1, "table of Vectors");
  Fixed dx, dy;
  dx = F_TO_Q(luaL_optnumber(L, 2, 0));
  dy = F_TO_Q(luaL_optnumber(L, 3, 0));
  int len = lua_objlen(L, 1);
  lua_settop(L, 1); // slight optimization, part 1
  CoordArray* ret = new CoordArray(len);
  Fixed* p = ret->coords;
  ret->Push(L); // Now it'll be deleted if we error out
  for(int n = 1; n <= len; ++n) {
    lua_rawgeti(L, 1, n);
    // all Vectors are at least Vec2s
    Vec2* vec = (Vec2*)lua_toobject(L, -1, Vector);
    *p++ = F_TO_Q(vec->x) + dx;
    *p++ = F_TO_Q(vec->y) + dy;
    lua_settop(L, 2); // slight optimization, part 2
  }
  return 1;
}

SUBCRITICAL_UTILITY(CompileVectorArray)(lua_State* L) {
  VectorArray* input = lua_toobject(L, 1, VectorArray);
  Fixed dx, dy;
  dx = F_TO_Q(luaL_optnumber(L, 2, 0));
  dy = F_TO_Q(luaL_optnumber(L, 3, 0));
  CoordArray* ret = new CoordArray(input->count);
  Fixed* p = ret->coords;
  ret->Push(L); // Now it'll be deleted if we error out
  for(uint32_t n = 0; n < input->count; ++n) {
    *p++ = F_TO_Q(input->buffer[n*input->order]) + dx;
    *p++ = F_TO_Q(input->buffer[n*input->order+1]) + dy;
  }
  return 1;
}

SUBCRITICAL_UTILITY(PerspectiveCompileVectors)(lua_State* L) {
  if(!lua_istable(L, 1)) return luaL_typerror(L, 1, "table of Vec3s");
  Fixed dx, dy;
  dx = F_TO_Q(luaL_optnumber(L, 2, 0));
  dy = F_TO_Q(luaL_optnumber(L, 3, 0));
  int len = lua_objlen(L, 1);
  lua_settop(L, 1); // slight optimization, part 1
  CoordArray* ret = new CoordArray(len);
  Fixed* p = ret->coords;
  ret->Push(L); // Now it'll be deleted if we error out
  for(int n = 1; n <= len; ++n) {
    lua_rawgeti(L, 1, n);
    Vec3* vec = (Vec3*)lua_toobject(L, -1, Vector);
    if(vec->n == 2) return luaL_error(L, "the table must contain Vec3s or Vec4s");
    Scalar rz = 1 / vec->z;
    *p++ = F_TO_Q(vec->x * rz) + dx;
    *p++ = F_TO_Q(vec->y * rz) + dy;
    lua_settop(L, 2); // slight optimization, part 2
  }
  return 1;
}

SUBCRITICAL_UTILITY(PerspectiveCompileVectorArray)(lua_State* L) {
  VectorArray* input = lua_toobject(L, 1, VectorArray);
  if(input->order == 2) return luaL_error(L, "the array must contain Vec3s or Vec4s");
  Fixed dx, dy;
  dx = F_TO_Q(luaL_optnumber(L, 2, 0));
  dy = F_TO_Q(luaL_optnumber(L, 3, 0));
  CoordArray* ret = new CoordArray(input->count);
  Fixed* p = ret->coords;
  ret->Push(L); // Now it'll be deleted if we error out
  for(uint32_t n = 0; n < input->count; ++n) {
    Scalar rz = 1 / input->buffer[n*input->order+2];
    *p++ = F_TO_Q(input->buffer[n*input->order] * rz) + dx;
    *p++ = F_TO_Q(input->buffer[n*input->order+1]* rz) + dy;
  }
  return 1;
}
