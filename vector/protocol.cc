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

#include "vector.h"

using namespace SubCritical;

PROTOCOL_IMP_PLAIN(MatrixOrVector, Object);
PROTOCOL_IMP_PLAIN(Vector, MatrixOrVector);

static const ObjectMethod V2M[] = {
  METHOD("Normalize", &Vec2::Normalize),
  METHOD("normalize", &Vec2::Normalize),
  METHOD("Magnitude", &Vec2::Magnitude),
  METHOD("MagnitudeSquared", &Vec2::MagnitudeSquared),
  NOMOREMETHODS(),
};
static const ObjectMethod V3M[] = {
  METHOD("Normalize", &Vec3::Normalize),
  METHOD("normalize", &Vec3::Normalize),
  METHOD("Magnitude", &Vec3::Magnitude),
  METHOD("MagnitudeSquared", &Vec3::MagnitudeSquared),
  NOMOREMETHODS(),
};
static const ObjectMethod V4M[] = {
  METHOD("Normalize", &Vec4::Normalize),
  METHOD("normalize", &Vec4::Normalize),
  METHOD("Magnitude", &Vec4::Magnitude),
  METHOD("MagnitudeSquared", &Vec4::MagnitudeSquared),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Vec2, Vector, V2M);
PROTOCOL_IMP(Vec3, Vector, V3M);
PROTOCOL_IMP(Vec4, Vector, V4M);

PROTOCOL_IMP_PLAIN(Matrix, MatrixOrVector);
PROTOCOL_IMP_PLAIN(Mat2x2, Matrix);
PROTOCOL_IMP_PLAIN(Mat2x3, Matrix);
PROTOCOL_IMP_PLAIN(Mat2x4, Matrix);
PROTOCOL_IMP_PLAIN(Mat3x2, Matrix);
PROTOCOL_IMP_PLAIN(Mat3x3, Matrix);
PROTOCOL_IMP_PLAIN(Mat3x4, Matrix);
PROTOCOL_IMP_PLAIN(Mat4x2, Matrix);
PROTOCOL_IMP_PLAIN(Mat4x3, Matrix);
PROTOCOL_IMP_PLAIN(Mat4x4, Matrix);

Vector::Vector(int n) : n(n) {}
Vec2::Vec2() : Vector(2) {}
Vec2::Vec2(Scalar x, Scalar y) : Vector(2), x(x), y(y) {}
Vec3::Vec3() : Vector(3) {}
Vec3::Vec3(Scalar x, Scalar y, Scalar z) : Vector(3), x(x), y(y), z(z) {}
Vec4::Vec4() : Vector(4) {}
Vec4::Vec4(Scalar x, Scalar y, Scalar z, Scalar w) : Vector(4), x(x), y(y), z(z), w(w) {}
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
