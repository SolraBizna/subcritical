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

// Try not to explode.

#if A > C
#define E A
#else
#define E C
#endif
#if B > D
#define F B
#else
#define F D
#endif

#define PRIME C4(Mat,E,x,F)
#if A > B
#define MAX_ARC A
#else
#define MAX_ARC B
#endif

#if C > D
#define MAX_BRC C
#else
#define MAX_BRC D
#endif

#if MAX_ARC > MAX_BRC
#define MAX_RC MAX_ARC
#else
#define MAX_RC MAX_BRC
#endif

#define RIGHT C4(Mat,C,x,D)

static PRIME* C6(mat_mul_,A,B,_,C,D)(const LEFT& a, const RIGHT& b) {
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
#define B_xx b.xx
#define B_xy b.xy
#if D >= 3
#define B_xz b.xz
#else
#define B_xz 0
#endif
#if D >= 4
#define B_xw b.xw
#else
#define B_xw 0
#endif
#define B_yx b.yx
#define B_yy b.yy
#if D >= 3
#define B_yz b.yz
#else
#define B_yz 0
#endif
#if D >= 4
#define B_yw b.yw
#else
#define B_yw 0
#endif
#if C >= 3
#define B_zx b.zx
#define B_zy b.zy
#if D >= 3
#define B_zz b.zz
#else
#define B_zz 1
#endif
#if D >= 4
#define B_zw b.zw
#else
#define B_zw 0
#endif
#else
#define B_zx 0
#define B_zy 0
#define B_zz 1
#define B_zw 0
#endif
#if C >= 4
#define B_wx b.wx
#define B_wy b.wy
#if D >= 3
#define B_wz b.wz
#else
#define B_wz 0
#endif
#if D >= 4
#define B_ww b.ww
#else
#define B_ww 1
#endif
#else
#define B_wx 0
#define B_wy 0
#define B_wz 0
#define B_ww 1
#endif
  PRIME* ret = new PRIME();
#define COL x
#define ROW x
#include "mat_sum_out.h"
#undef ROW
#define ROW y
#include "mat_sum_out.h"
#undef ROW
#if E >= 3
#define ROW z
#include "mat_sum_out.h"
#undef ROW
#if E >= 4
#define ROW w
#include "mat_sum_out.h"
#undef ROW
#endif
#endif
#undef COL
#define COL y
#define ROW x
#include "mat_sum_out.h"
#undef ROW
#define ROW y
#include "mat_sum_out.h"
#undef ROW
#if E >= 3
#define ROW z
#include "mat_sum_out.h"
#undef ROW
#if E >= 4
#define ROW w
#include "mat_sum_out.h"
#undef ROW
#endif
#endif
#undef COL
#if F >= 3
#define COL z
#define ROW x
#include "mat_sum_out.h"
#undef ROW
#define ROW y
#include "mat_sum_out.h"
#undef ROW
#if E >= 3
#define ROW z
#include "mat_sum_out.h"
#undef ROW
#if E >= 4
#define ROW w
#include "mat_sum_out.h"
#undef ROW
#endif
#endif
#undef COL
#if F >= 4
#define COL w
#define ROW x
#include "mat_sum_out.h"
#undef ROW
#define ROW y
#include "mat_sum_out.h"
#undef ROW
#if E >= 3
#define ROW z
#include "mat_sum_out.h"
#undef ROW
#if E >= 4
#define ROW w
#include "mat_sum_out.h"
#undef ROW
#endif
#endif
#undef COL
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
#undef B_xx
#undef B_xy
#undef B_xz
#undef B_xw
#undef B_yx
#undef B_yy
#undef B_yz
#undef B_yw
#undef B_zx
#undef B_zy
#undef B_zz
#undef B_zw
#undef B_wx
#undef B_wy
#undef B_wz
#undef B_ww
  return ret;
}

#undef E
#undef F
#undef PRIME

#undef RIGHT
#undef MAX_RC
#undef MAX_ARC
#undef MAX_BRC
