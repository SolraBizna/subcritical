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

#define PRIME C2(Vec,A)
#define RIGHT C2(Vec,C)

static PRIME*restrict C5(mat_mul_,A,B,_,C)(const LEFT&restrict a, const RIGHT&restrict b) {
#define A_xx a.xx
#define A_xy a.xy
#if B >= 3
#define A_xz a.xz
#else
#define A_xz 0
#endif
#if B >= 4
#define A_xw a.xw
#else
#define A_xw 0
#endif
#define A_yx a.yx
#define A_yy a.yy
#if B >= 3
#define A_yz a.yz
#else
#define A_yz 0
#endif
#if B >= 4
#define A_yw a.yw
#else
#define A_yw 0
#endif
#if A >= 3
#define A_zx a.zx
#define A_zy a.zy
#if B >= 3
#define A_zz a.zz
#else
#define A_zz 1
#endif
#if B >= 4
#define A_zw a.zw
#else
#define A_zw 0
#endif
#else
#define A_zx 0
#define A_zy 0
#define A_zz 1
#define A_zw 0
#endif
#if A >= 4
#define A_wx a.wx
#define A_wy a.wy
#if B >= 3
#define A_wz a.wz
#else
#define A_wz 0
#endif
#if B >= 4
#define A_ww a.ww
#else
#define A_ww 1
#endif
#else
#define A_wx 0
#define A_wy 0
#define A_wz 0
#define A_ww 1
#endif
#define B_x b.x
#define B_y b.y
#if C >= 3
#define B_z b.z
#else
#define B_z 0
#endif
#if C >= 4
#define B_w b.w
#else
#define B_w 1
#endif
  PRIME*restrict ret = new PRIME();
  ret->x = (A_xx * B_x) + (A_xy * B_y)
#if C >= 3 || B >= 3
    + (A_xz * B_z)
#if C >= 4 || B >= 4
    + (A_xw * B_w)
#endif
#endif
    ;
  ret->y = (A_yx * B_x) + (A_yy * B_y)
#if C >= 3 || B >= 3
    + (A_yz * B_z)
#if C >= 4 || B >= 4
    + (A_yw * B_w)
#endif
#endif
    ;
#if A >= 3
  ret->z = (A_zx * B_x) + (A_zy * B_y)
#if C >= 3 || B >= 3
    + (A_zz * B_z)
#if C >= 4 || B >= 4
    + (A_zw * B_w)
#endif
#endif
    ;
#if A >= 4
  ret->w = (A_wx * B_x) + (A_wy * B_y)
#if C >= 3 || B >= 3
    + (A_wz * B_z)
#if C >= 4 || B >= 4
    + (A_ww * B_w)
#endif
#endif
    ;
#endif
#endif
#undef A_xx
#undef A_xy
#undef A_xz
#undef A_xw
#undef A_yx
#undef A_yy
#undef A_yz
#undef A_yw
#undef A_zx
#undef A_zy
#undef A_zz
#undef A_zw
#undef A_wx
#undef A_wy
#undef A_wz
#undef A_ww
#undef B_x
#undef B_y
#undef B_z
#undef B_w
  return ret;
}

static void C5(mat_mul_,A,B,_D,C)(const LEFT&restrict a, const Scalar*restrict b, Scalar* out) {
#define A_xx a.xx
#define A_xy a.xy
#if B >= 3
#define A_xz a.xz
#else
#define A_xz 0
#endif
#if B >= 4
#define A_xw a.xw
#else
#define A_xw 0
#endif
#define A_yx a.yx
#define A_yy a.yy
#if B >= 3
#define A_yz a.yz
#else
#define A_yz 0
#endif
#if B >= 4
#define A_yw a.yw
#else
#define A_yw 0
#endif
#if A >= 3
#define A_zx a.zx
#define A_zy a.zy
#if B >= 3
#define A_zz a.zz
#else
#define A_zz 1
#endif
#if B >= 4
#define A_zw a.zw
#else
#define A_zw 0
#endif
#else
#define A_zx 0
#define A_zy 0
#define A_zz 1
#define A_zw 0
#endif
#if A >= 4
#define A_wx a.wx
#define A_wy a.wy
#if B >= 3
#define A_wz a.wz
#else
#define A_wz 0
#endif
#if B >= 4
#define A_ww a.ww
#else
#define A_ww 1
#endif
#else
#define A_wx 0
#define A_wy 0
#define A_wz 0
#define A_ww 1
#endif
#define B_x b[0]
#define B_y b[1]
#if C >= 3
#define B_z b[2]
#else
#define B_z 0
#endif
#if C >= 4
#define B_w b[3]
#else
#define B_w 1
#endif
  out[0] = (A_xx * B_x) + (A_xy * B_y)
#if C >= 3 || B >= 3
    + (A_xz * B_z)
#if C >= 4 || B >= 4
    + (A_xw * B_w)
#endif
#endif
    ;
  out[1] = (A_yx * B_x) + (A_yy * B_y)
#if C >= 3 || B >= 3
    + (A_yz * B_z)
#if C >= 4 || B >= 4
    + (A_yw * B_w)
#endif
#endif
    ;
#if A >= 3
  out[2] = (A_zx * B_x) + (A_zy * B_y)
#if C >= 3 || B >= 3
    + (A_zz * B_z)
#if C >= 4 || B >= 4
    + (A_zw * B_w)
#endif
#endif
    ;
#if A >= 4
  out[3] = (A_wx * B_x) + (A_wy * B_y)
#if C >= 3 || B >= 3
    + (A_wz * B_z)
#if C >= 4 || B >= 4
    + (A_ww * B_w)
#endif
#endif
    ;
#endif
#endif
#undef A_xx
#undef A_xy
#undef A_xz
#undef A_xw
#undef A_yx
#undef A_yy
#undef A_yz
#undef A_yw
#undef A_zx
#undef A_zy
#undef A_zz
#undef A_zw
#undef A_wx
#undef A_wy
#undef A_wz
#undef A_ww
#undef B_x
#undef B_y
#undef B_z
#undef B_w
}

#undef RIGHT
#undef PRIME
