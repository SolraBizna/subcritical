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

#include "vector.h"

#include <stdlib.h>
#include <new>

using namespace SubCritical;

PROTOCOL_IMP_PLAIN(MatrixOrVectorOrVectorArray, Object);
PROTOCOL_IMP_PLAIN(Vector, MatrixOrVectorOrVectorArray);

static const ObjectMethod V2M[] = {
  METHOD("Normalize", &Vec2::Normalize),
  METHOD("normalize", &Vec2::Normalize),
  METHOD("Magnitude", &Vec2::Magnitude),
  METHOD("MagnitudeSquared", &Vec2::MagnitudeSquared),
  METHOD("Angle", &Vec2::Angle),
  METHOD("AngleXY", &Vec2::Angle),
  NOMOREMETHODS(),
};
static const ObjectMethod V3M[] = {
  METHOD("Normalize", &Vec3::Normalize),
  METHOD("normalize", &Vec3::Normalize),
  METHOD("Magnitude", &Vec3::Magnitude),
  METHOD("MagnitudeSquared", &Vec3::MagnitudeSquared),
  METHOD("Angle", &Vec3::AngleXY),
  METHOD("AngleXY", &Vec3::AngleXY),
  METHOD("AngleXZ", &Vec3::AngleXZ),
  METHOD("AngleYZ", &Vec3::AngleYZ),
  NOMOREMETHODS(),
};
static const ObjectMethod V4M[] = {
  METHOD("Normalize", &Vec4::Normalize),
  METHOD("normalize", &Vec4::Normalize),
  METHOD("Magnitude", &Vec4::Magnitude),
  METHOD("MagnitudeSquared", &Vec4::MagnitudeSquared),
  METHOD("Angle", &Vec4::AngleXY),
  METHOD("AngleXY", &Vec4::AngleXY),
  METHOD("AngleXZ", &Vec4::AngleXZ),
  METHOD("AngleYZ", &Vec4::AngleYZ),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Vec2, Vector, V2M);
PROTOCOL_IMP(Vec3, Vector, V3M);
PROTOCOL_IMP(Vec4, Vector, V4M);

static const ObjectMethod VAM[] = {
  METHOD("Get", &VectorArray::Lua_Get),
  METHOD("GetCount", &VectorArray::Lua_GetCount),
  METHOD("GetOrder", &VectorArray::Lua_GetOrder),
  METHOD("UnrolledGet", &VectorArray::Lua_UnrolledGet),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(VectorArray, MatrixOrVectorOrVectorArray, VAM);

#define SMALLMETHODS(Mat) \
  METHOD("MultiplyAndCompile", &Mat::MultiplyAndCompile),
#define LARGEMETHODS(Mat) \
  METHOD("PerspectiveMultiplyAndCompile", &Mat::PerspectiveMultiplyAndCompile),
#define MAT_METHODS_FULL(Mat) \
static const ObjectMethod Met_##Mat[] = { \
SMALLMETHODS(Mat) \
LARGEMETHODS(Mat) \
  NOMOREMETHODS(), \
}; \
PROTOCOL_IMP(Mat, Matrix, Met_##Mat)
#define MAT_METHODS_SMALL(Mat) \
static const ObjectMethod Met_##Mat[] = { \
SMALLMETHODS(Mat) \
  NOMOREMETHODS(), \
}; \
PROTOCOL_IMP(Mat, Matrix, Met_##Mat)

PROTOCOL_IMP_PLAIN(Matrix, MatrixOrVectorOrVectorArray);
MAT_METHODS_SMALL(Mat2x2);
MAT_METHODS_SMALL(Mat2x3);
MAT_METHODS_SMALL(Mat2x4);
MAT_METHODS_FULL(Mat3x2);
MAT_METHODS_FULL(Mat3x3);
MAT_METHODS_FULL(Mat3x4);
MAT_METHODS_FULL(Mat4x2);
MAT_METHODS_FULL(Mat4x3);
MAT_METHODS_FULL(Mat4x4);

Vector::Vector(int n) : n(n) {}
Vec2::Vec2() : Vector(2) {}
Vec2::Vec2(Scalar x, Scalar y) : Vector(2), x(x), y(y) {}
Vec3::Vec3() : Vector(3) {}
Vec3::Vec3(Scalar x, Scalar y, Scalar z) : Vector(3), x(x), y(y), z(z) {}
Vec4::Vec4() : Vector(4) {}
Vec4::Vec4(Scalar x, Scalar y, Scalar z, Scalar w) : Vector(4), x(x), y(y), z(z), w(w) {}

VectorArray::VectorArray(uint32_t order, uint32_t count) : order(order),count(count) {
  buffer = (Scalar*)calloc(order*sizeof(Scalar), count);
  if(!buffer) throw std::bad_alloc();
}
VectorArray::~VectorArray() {
  if(buffer) {
    free(buffer);
    buffer = NULL;
  }
}

Matrix::Matrix(int r, int c) : r(r),c(c) {}

SUBCRITICAL_CONSTRUCTOR(Vec2)(lua_State* L) {
  if(lua_gettop(L) == 0)
    (new Vec2(0, 0))->Push(L);
  else
    (new Vec2(luaL_checknumber(L, 1), luaL_checknumber(L, 2)))->Push(L);
  return 1;
}

SUBCRITICAL_CONSTRUCTOR(Vec3)(lua_State* L) {
  if(lua_gettop(L) == 0)
    (new Vec3(0, 0, 0))->Push(L);
  else
    (new Vec3(luaL_checknumber(L, 1), luaL_checknumber(L, 2),
	      luaL_checknumber(L, 3)))->Push(L);
  return 1;
}

SUBCRITICAL_CONSTRUCTOR(Vec4)(lua_State* L) {
  if(lua_gettop(L) == 0)
    (new Vec4(0, 0, 0, 1))->Push(L);
  else
    (new Vec4(luaL_checknumber(L, 1), luaL_checknumber(L, 2),
	      luaL_checknumber(L, 3), luaL_checknumber(L, 4)))->Push(L);
  return 1;
}

#define RECURSIVE_MATRIX_BODY "matrix_scconstruct.h"
#include "recursive_matrix.h"
#undef RECURSIVE_MATRIX_BODY

SUBCRITICAL_UTILITY(Vec)(lua_State* L) {
  if(lua_istable(L, 1)) {
    fromtabletovector(L, 1)->Push(L);
    return 1;
  }
  else return luaL_typerror(L, 1, "table");
}

/* these don't really belong here... */
int VectorArray::Lua_GetCount(lua_State* L) {
  lua_pushinteger(L, count);
  return 1;
}

int VectorArray::Lua_GetOrder(lua_State* L) {
  lua_pushinteger(L, order);
  return 1;
}

int VectorArray::Lua_Get(lua_State* L) {
  lua_Integer index = luaL_checkinteger(L, 1);
  if(index < 0 || (size_t)index >= count) return luaL_error(L, "Get index out of range");
  switch(order) {
  default: return luaL_error(L, "Unknown order!?");
  case 2:
    (new Vec2(buffer[index*2], buffer[index*2+1]))->Push(L);
    break;
  case 3:
    (new Vec3(buffer[index*3], buffer[index*3+1], buffer[index*3+2]))->Push(L);
    break;
  case 4:
    (new Vec4(buffer[index*4], buffer[index*4+1], buffer[index*4+2], buffer[index*4+3]))->Push(L);
    break;
  }
  return 1;
}

int VectorArray::Lua_UnrolledGet(lua_State* L) {
  lua_Integer index = luaL_checkinteger(L, 1);
  if(index < 0 || (size_t)index >= count) return luaL_error(L, "UnrolledGet index out of range");
  switch(order) {
  default: return luaL_error(L, "Unknown order!?");
  case 2:
    lua_pushnumber(L, buffer[index*2]);
    lua_pushnumber(L, buffer[index*2+1]);
    return 2;
  case 3:
    lua_pushnumber(L, buffer[index*3]);
    lua_pushnumber(L, buffer[index*3+1]);
    lua_pushnumber(L, buffer[index*3+2]);
    return 3;
  case 4:
    lua_pushnumber(L, buffer[index*4]);
    lua_pushnumber(L, buffer[index*4+1]);
    lua_pushnumber(L, buffer[index*4+2]);
    lua_pushnumber(L, buffer[index*4+3]);
    return 4;
  }
}
