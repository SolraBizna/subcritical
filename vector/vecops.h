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

#include <math.h>

#include "vec2.h"
#include "vec3.h"
#include "vec4.h"

typedef Vector*restrict(*op_vvv)(const Vector&restrict, const Vector&restrict);
typedef Vector*restrict(*op_vvs)(const Vector&restrict, Scalar);
typedef Scalar(*op_svv)(const Vector&restrict, const Vector&restrict);
typedef bool(*op_bvv)(const Vector&restrict, const Vector&restrict);
typedef Vector*restrict(*op_vv)(const Vector&restrict);
typedef int(*op_Lv)(lua_State*, const Vector&);
typedef Scalar(*op_sLvn)(lua_State*, const Vector&, int);
typedef int(*op_Lvns)(lua_State*, Vector&, int, Scalar);

static const op_vvv vec_add[3][3] = {
  {(op_vvv)vec_add_222,(op_vvv)vec_add_323,(op_vvv)vec_add_424},
  {(op_vvv)vec_add_332,(op_vvv)vec_add_333,(op_vvv)vec_add_434},
  {(op_vvv)vec_add_442,(op_vvv)vec_add_443,(op_vvv)vec_add_444},
};

static const op_vvv vec_sub[3][3] = {
  {(op_vvv)vec_sub_222,(op_vvv)vec_sub_323,(op_vvv)vec_sub_424},
  {(op_vvv)vec_sub_332,(op_vvv)vec_sub_333,(op_vvv)vec_sub_434},
  {(op_vvv)vec_sub_442,(op_vvv)vec_sub_443,(op_vvv)vec_sub_444},
};

static const op_svv vec_concat[3][3] = {
  {(op_svv)vec_concat_222,(op_svv)vec_concat_323,(op_svv)vec_concat_424},
  {(op_svv)vec_concat_332,(op_svv)vec_concat_333,(op_svv)vec_concat_434},
  {(op_svv)vec_concat_442,(op_svv)vec_concat_443,(op_svv)vec_concat_444},
};

static const op_vvs vec_mul[3] = {
  (op_vvs)vec_mul_2,(op_vvs)vec_mul_3,(op_vvs)vec_mul_4,
};

static const op_Lv vec_unpack[3] = {
  (op_Lv)vec_unpack_2,(op_Lv)vec_unpack_3,(op_Lv)vec_unpack_4,
};

static const op_bvv vec_eq[3] = {
  (op_bvv)vec_eq_2,(op_bvv)vec_eq_3,(op_bvv)vec_eq_4,
};

static const op_vv vec_unm[3] = {
  (op_vv)vec_unm_2,(op_vv)vec_unm_3,(op_vv)vec_unm_4,
};

static const op_sLvn vec_index[3] = {
  (op_sLvn)vec_index_2,(op_sLvn)vec_index_3,(op_sLvn)vec_index_4,
};

static const op_Lvns vec_newindex[3] = {
  (op_Lvns)vec_newindex_2,(op_Lvns)vec_newindex_3,(op_Lvns)vec_newindex_4,
};

LOCAL Vector* fromtabletovector(lua_State* L, int n) {
  Vector* ret = NULL;
  int order = lua_objlen(L, n);
  switch(order) {
  case 2:
    lua_rawgeti(L, n, 1);
    lua_rawgeti(L, n, 2);
    ret = new Vec2(luaL_checknumber(L, -2), luaL_checknumber(L, -1));
    lua_pop(L, 2);
    break;
  case 3:
    lua_rawgeti(L, n, 1);
    lua_rawgeti(L, n, 2);
    lua_rawgeti(L, n, 3);
    ret = new Vec3(luaL_checknumber(L, -3), luaL_checknumber(L, -2), luaL_checknumber(L, -1));
    lua_pop(L, 3);
    break;
  case 4:
    lua_rawgeti(L, n, 1);
    lua_rawgeti(L, n, 2);
    lua_rawgeti(L, n, 3);
    lua_rawgeti(L, n, 4);
    ret = new Vec4(luaL_checknumber(L, -4), luaL_checknumber(L, -3), luaL_checknumber(L, -2), luaL_checknumber(L, -1));
    lua_pop(L, 4);
    break;
  default:
    luaL_typerror(L, n, "table with 2, 3, or 4 elements");
  }
  return ret;
}

static int f_vector_add(lua_State* L) {
  Vector*restrict a = lua_toobject(L, 1, Vector);
  if(lua_istable(L, 2)) {
    Vector*restrict b = fromtabletovector(L, 2);
    Vector*restrict c = (vec_add[a->n-2][b->n-2])(*a, *b);
    delete b;
    c->Push(L);
    return 1;
  }
  else {
    Vector*restrict b = lua_toobject(L, 2, Vector);
    Vector*restrict c = (vec_add[a->n-2][b->n-2])(*a, *b);
    c->Push(L);
    return 1;
  }
}

static int f_vector_sub(lua_State* L) {
  Vector*restrict a = lua_toobject(L, 1, Vector);
  if(lua_istable(L, 2)) {
    Vector*restrict b = fromtabletovector(L, 2);
    Vector*restrict c = (vec_sub[a->n-2][b->n-2])(*a, *b);
    delete b;
    c->Push(L);
    return 1;
  }
  else {
    Vector*restrict b = lua_toobject(L, 2, Vector);
    Vector*restrict c = (vec_sub[a->n-2][b->n-2])(*a, *b);
    c->Push(L);
    return 1;
  }
}

static int f_vector_concat(lua_State* L) {
  Vector*restrict a = lua_toobject(L, 1, Vector);
  if(lua_istable(L, 2)) {
    Vector*restrict b = fromtabletovector(L, 2);
    lua_pushnumber(L, (vec_concat[a->n-2][b->n-2])(*a, *b));
    delete b;
    return 1;
  }
  else {
    Vector*restrict b = lua_toobject(L, 2, Vector);
    lua_pushnumber(L, (vec_concat[a->n-2][b->n-2])(*a, *b));
    return 1;
  }
}

static int f_vector_mul(lua_State* L) {
  Vector*restrict a = lua_toobject(L, 1, Vector);
  Scalar b = luaL_checknumber(L, 2);
  Vector*restrict c = (vec_mul[a->n-2])(*a, b);
  c->Push(L);
  return 1;
}

static int f_vector_unpack(lua_State* L) {
  Vector*restrict a = lua_toobject(L, 1, Vector);
  return (vec_unpack[a->n-2])(L, *a);
}

static int f_vector_eq(lua_State* L) {
  Vector*restrict a = lua_toobject(L, 1, Vector);
  if(lua_istable(L, 2)) {
    Vector*restrict b = fromtabletovector(L, 2);
    if(a->n != b->n)
      lua_pushboolean(L, 0);
    else
      lua_pushboolean(L, (vec_eq[a->n-2])(*a, *b));
    delete b;
    return 1;
  }
  else {
    Vector*restrict b = lua_toobject(L, 2, Vector);
    if(a->n != b->n)
      lua_pushboolean(L, 0);
    else
      lua_pushboolean(L, (vec_eq[a->n-2])(*a, *b));
    return 1;
  }
}

static int f_vector_unm(lua_State* L) {
  Vector* a = lua_toobject(L, 1, Vector);
  (vec_unm[a->n-2])(*a)->Push(L);
  return 1;
}

static int f_vector_index(lua_State* L) {
  Vector* a = lua_toobject(L, 1, Vector);
  if(lua_isnumber(L, 2)) {
    switch(lua_tointeger(L, 2)) {
    case 1:
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 1));
      return 1;
    case 2:
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 2));
      return 1;
    case 3:
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 3));
      return 1;
    case 4:
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 4));
      return 1;
    }
  }
  else if(lua_isstring(L, 2) && lua_objlen(L, 2) == 1) {
    switch(*lua_tostring(L, 2)) {
    case 'x': case 'X':
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 1));
      return 1;
    case 'y': case 'Y':
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 2));
      return 1;
    case 'z': case 'Z':
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 3));
      return 1;
    case 'w': case 'W':
      lua_pushnumber(L, (vec_index[a->n-2])(L, *a, 4));
      return 1;
    }
  }
  lua_getmetatable(L, 1);
  lua_getfield(L, -1, "__oldindex");
  lua_replace(L, 1);
  lua_settop(L, 2);
  lua_gettable(L, 1);
  return 1;
}

static int f_vector_newindex(lua_State* L) {
  Vector* a = lua_toobject(L, 1, Vector);
  Scalar s = luaL_checknumber(L, 3);
  if(lua_isnumber(L, 2)) {
    switch(lua_tointeger(L, 2)) {
    case 1:
      return (vec_newindex[a->n-2])(L, *a, 1, s);
    case 2:
      return (vec_newindex[a->n-2])(L, *a, 2, s);
    case 3:
      return (vec_newindex[a->n-2])(L, *a, 3, s);
    case 4:
      return (vec_newindex[a->n-2])(L, *a, 4, s);
    }
  }
  else if(lua_isstring(L, 2) && lua_objlen(L, 2) == 1) {
    switch(*lua_tostring(L, 2)) {
    case 'x': case 'X':
      return (vec_newindex[a->n-2])(L, *a, 1, s);
    case 'y': case 'Y':
      return (vec_newindex[a->n-2])(L, *a, 2, s);
    case 'z': case 'Z':
      return (vec_newindex[a->n-2])(L, *a, 3, s);
    case 'w': case 'W':
      return (vec_newindex[a->n-2])(L, *a, 4, s);
    }
  }
  return luaL_error(L, "Attempt to set a weird vector element");
}

static const struct luaL_Reg vector_ops[] = {
  {"__add", f_vector_add},
  {"__sub", f_vector_sub},
  {"__concat", f_vector_concat},
  {"__mul", f_vector_mul},
  {"__call", f_vector_unpack},
  {"__eq", f_vector_eq},
  {"__unm", f_vector_unm},
  {"__index", f_vector_index},
  {"__newindex", f_vector_newindex},
  {NULL, NULL}
};

static void populate_vector_ops(lua_State* L, int n) {
  lua_getmetatable(L, n);
  lua_getfield(L, -1, "__index");
  lua_setfield(L, -2, "__oldindex");
  luaL_register(L, NULL, vector_ops);
  lua_pop(L, 1);
}
