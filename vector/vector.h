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

#ifndef VECTOR_H
#define VECTOR_H

#include "subcritical/core.h"
#include "subcritical/graphics.h"

#define _CPP(x) x
#define CPP(x) _CPP(x)
#define _C6(a,b,c,d,e,f) a##b##c##d##e##f
#define C2(a,b) _C6(a,b,,,,)
#define C3(a,b,c) _C6(a,b,c,,,)
#define C4(a,b,c,d) _C6(a,b,c,d,,)
#define C5(a,b,c,d,e) _C6(a,b,c,d,e,)
#define C6(a,b,c,d,e,f) _C6(a,b,c,d,e,f)

namespace SubCritical {
  typedef lua_Number Scalar;
  class LOCAL MatrixOrVectorOrVectorArray : public Object {
  public:
    PROTOCOL_PROTOTYPE();
  };
  class LOCAL Vector : public MatrixOrVectorOrVectorArray {
  public:
    PROTOCOL_PROTOTYPE();
    Vector(int n);
    // making this virtual would add execution overhead and waste between 4 and
    // 20 bytes per Vector instance
    //virtual int Normalize(lua_State* L);
    int n;
  };
  class LOCAL Vec2 : public Vector {
  public:
    PROTOCOL_PROTOTYPE();
    Vec2();
    Vec2(Scalar x, Scalar y);
    int Normalize(lua_State* L);
    int Magnitude(lua_State* L);
    int MagnitudeSquared(lua_State* L);
    int Angle(lua_State* L);
    Scalar x, y;
  };
  class LOCAL Vec3 : public Vector {
  public:
    PROTOCOL_PROTOTYPE();
    Vec3();
    Vec3(Scalar x, Scalar y, Scalar z = 0);
    int Normalize(lua_State* L);
    int Magnitude(lua_State* L);
    int MagnitudeSquared(lua_State* L);
    int AngleXY(lua_State* L);
    int AngleXZ(lua_State* L);
    int AngleYZ(lua_State* L);
    Scalar x, y, z;
  };
  class LOCAL Vec4 : public Vector {
  public:
    PROTOCOL_PROTOTYPE();
    Vec4();
    Vec4(Scalar x, Scalar y, Scalar z = 0, Scalar w = 1);
    int Normalize(lua_State* L);
    int Magnitude(lua_State* L);
    int MagnitudeSquared(lua_State* L);
    int AngleXY(lua_State* L);
    int AngleXZ(lua_State* L);
    int AngleYZ(lua_State* L);
    Scalar x, y, z, w;
  };
  class LOCAL VectorArray : public Object {
  public:
    PROTOCOL_PROTOTYPE();
    VectorArray(uint32_t order, uint32_t count);
    ~VectorArray();
    int Lua_GetCount(lua_State* L);
    int Lua_GetOrder(lua_State* L);
    int Lua_Get(lua_State* L);
    int Lua_UnrolledGet(lua_State* L);
    uint32_t order, count;
    Scalar* buffer;
  };
  class LOCAL Matrix : public MatrixOrVectorOrVectorArray {
  public:
    PROTOCOL_PROTOTYPE();
    Matrix(int r, int c);
    int r, c;
  };
  class LOCAL Mat2x2 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat2x2();
    Mat2x2(Scalar[2*2]);
    int MultiplyAndCompile(lua_State* L);
    Scalar xx, yx,
           xy, yy;
  };
  class LOCAL Mat2x3 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat2x3();
    Mat2x3(Scalar[2*3]);
    int MultiplyAndCompile(lua_State* L);
    Scalar xx, yx,
           xy, yy,
           xz, yz;
  };
  class LOCAL Mat2x4 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat2x4();
    Mat2x4(Scalar[2*4]);
    int MultiplyAndCompile(lua_State* L);
    Scalar xx, yx,
           xy, yy,
           xz, yz,
           xw, yw;
  };
  class LOCAL Mat3x2 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat3x2();
    Mat3x2(Scalar[3*2]);
    int MultiplyAndCompile(lua_State* L);
    int PerspectiveMultiplyAndCompile(lua_State* L);
    Scalar xx, yx, zx,
           xy, yy, zy;
  };
  class LOCAL Mat3x3 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat3x3();
    Mat3x3(Scalar[3*3]);
    int MultiplyAndCompile(lua_State* L);
    int PerspectiveMultiplyAndCompile(lua_State* L);
    Scalar xx, yx, zx,
           xy, yy, zy,
           xz, yz, zz;
  };
  class LOCAL Mat3x4 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat3x4();
    Mat3x4(Scalar[3*4]);
    int MultiplyAndCompile(lua_State* L);
    int PerspectiveMultiplyAndCompile(lua_State* L);
    Scalar xx, yx, zx,
           xy, yy, zy,
           xz, yz, zz,
           xw, yw, zw;
  };
  class LOCAL Mat4x2 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat4x2();
    Mat4x2(Scalar[4*2]);
    int MultiplyAndCompile(lua_State* L);
    int PerspectiveMultiplyAndCompile(lua_State* L);
    Scalar xx, yx, zx, wx,
           xy, yy, zy, wy;
  };
  class LOCAL Mat4x3 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat4x3();
    Mat4x3(Scalar[4*3]);
    int MultiplyAndCompile(lua_State* L);
    int PerspectiveMultiplyAndCompile(lua_State* L);
    Scalar xx, yx, zx, wx,
           xy, yy, zy, wy,
           xz, yz, zz, wz;
  };
  class LOCAL Mat4x4 : public Matrix {
  public:
    PROTOCOL_PROTOTYPE();
    Mat4x4();
    Mat4x4(Scalar[4*4]);
    int MultiplyAndCompile(lua_State* L);
    int PerspectiveMultiplyAndCompile(lua_State* L);
    Scalar xx, yx, zx, wx,
           xy, yy, zy, wy,
           xz, yz, zz, wz,
           xw, yw, zw, ww;
  };
}

LOCAL SubCritical::Vector* fromtabletovector(lua_State* L, int n);

#endif
