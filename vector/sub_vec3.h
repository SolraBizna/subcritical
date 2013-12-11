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

#if an >= 3
#define az a.z
#else
#define az 0
#endif

#if bn >= 3
#define bz b.z
#else
#define bz 0
#endif

static Vec3*restrict C3(vec_add_3,an,bn)(const A&restrict a, const B&restrict b) {
  Vec3*restrict ret = new Vec3();
  ret->x = a.x + b.x;
  ret->y = a.y + b.y;
  ret->z = az + bz;
  return ret;
}

static Vec3*restrict C3(vec_sub_3,an,bn)(const A&restrict a, const B&restrict b) {
  Vec3*restrict ret = new Vec3();
  ret->x = a.x - b.x;
  ret->y = a.y - b.y;
  ret->z = az - bz;
  return ret;
}

static Scalar C3(vec_concat_3,an,bn)(const A&restrict a, const B&restrict b) {
  return a.x*b.x + a.y*b.y + az*bz;
}

#undef az
#undef bz
