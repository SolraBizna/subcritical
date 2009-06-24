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

#if an >= 3
#define az a.z
#if an >= 4
#define aw a.w
#else
#define aw 1
#endif
#else
#define az 0
#define aw 1
#endif

#if bn >= 3
#define bz b.z
#if bn >= 4
#define bw b.w
#else
#define bw 1
#endif
#else
#define bz 0
#define bw 1
#endif

static Vec4*restrict C3(vec_add_4,an,bn)(const A&restrict a, const B&restrict b) {
  Vec4*restrict ret = new Vec4();
  ret->x = a.x + b.x;
  ret->y = a.y + b.y;
  ret->z = az + bz;
  ret->w = aw + bw;
  return ret;
}

static Vec4*restrict C3(vec_sub_4,an,bn)(const A&restrict a, const B&restrict b) {
  Vec4*restrict ret = new Vec4();
  ret->x = a.x - b.x;
  ret->y = a.y - b.y;
  ret->z = az - bz;
  ret->w = aw - bw;
  return ret;
}

static Scalar C3(vec_concat_4,an,bn)(const A&restrict a, const B&restrict b) {
  return a.x*b.x + a.y*b.y + az*bz + aw*bw;
}

#undef az
#undef aw
#undef bz
#undef bw
