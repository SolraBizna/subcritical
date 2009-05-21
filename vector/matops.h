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

#define SPLAT_A "splat_b.h"
#define SPLAT_B "mat_straight.h"
#include "splat_a.h"
#undef SPLAT_A
#undef SPLAT_B

typedef Matrix*restrict(*op_mmm)(const Matrix&restrict, const Matrix&restrict);
typedef Vector*restrict(*op_vmv)(const Matrix&, const Vector&restrict);
typedef int(*op_Lm)(lua_State* L, const Matrix&);

static const op_Lm mat_unpack[9] = {
#define SPLAT_A "splat_b.h"
#define SPLAT_B "splat_out.h"
#define SPLAT_OUT (op_Lm)C4(mat_unpack_,A,x,B),
#include "splat_a.h"
#undef SPLAT_A
#undef SPLAT_B
#undef SPLAT_OUT
};

static const op_mmm mat_mul[81] = {
#define SPLAT_A "splat_b.h"
#define SPLAT_B "splat_c.h"
#define SPLAT_C "splat_d.h"
#define SPLAT_D "splat_out.h"
#define SPLAT_OUT (op_mmm)C6(mat_mul_,A,B,_,C,D),
#include "splat_a.h"
#undef SPLAT_A
#undef SPLAT_B
#undef SPLAT_C
#undef SPLAT_D
#undef SPLAT_OUT
};

static const op_vmv mat_mul_v[27] = {
#define SPLAT_A "splat_b.h"
#define SPLAT_B "splat_c.h"
#define SPLAT_C "splat_out.h"
#define SPLAT_OUT (op_vmv)C5(mat_mul_,A,B,_,C),
#include "splat_a.h"
#undef SPLAT_A
#undef SPLAT_B
#undef SPLAT_C
#undef SPLAT_OUT
};

static int f_matrix_unpack(lua_State* L) {
  Matrix* a = lua_toobject(L, 1, Matrix);
  return (mat_unpack[(a->c-2) + (a->r-2)*3])(L, *a);
}

static int f_matrix_mul(lua_State* L) {
  Matrix*restrict a = lua_toobject(L, 1, Matrix);
  MatrixOrVector*restrict b_ = lua_toobject(L, 2, MatrixOrVector);
  if(b_->IsA("Matrix")) {
    Matrix*restrict b = (Matrix*)b_;
    Matrix*restrict ret = (mat_mul[(b->c-2) + (b->r-2)*3 + (a->c-2)*9 + (a->r-2)*27])(*a, *b);
    ret->Push(L);
    return 1;
  }
  else {
    Vector*restrict b = (Vector*)b_;
    Vector*restrict ret = (mat_mul_v[(b->n-2) + (a->c-2)*3 + (a->r-2)*9])(*a, *b);
    ret->Push(L);
    return 1;
  }
}

static const struct luaL_Reg matrix_ops[] = {
  {"__mul", f_matrix_mul},
  {"__call", f_matrix_unpack},
  //{"__unm", f_matrix_unm},
  {NULL, NULL}
};

static void populate_matrix_ops(lua_State* L, int n) {
  lua_getmetatable(L, n);
  luaL_register(L, NULL, matrix_ops);
  lua_pop(L, 1);
}
