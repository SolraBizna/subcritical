/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2014 Solra Bizna.

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

#include "vecops.h"
#include "matops.h"

#include "transforms.h"

LUA_EXPORT int Init_vector(lua_State* L) {
  (new Vec2())->Push(L);
  populate_vector_ops(L, -1);
  (new Vec3())->Push(L);
  populate_vector_ops(L, -1);
  (new Vec4())->Push(L);
  populate_vector_ops(L, -1);
#define SPLAT_A "splat_b.h"
#define SPLAT_B "populate_matrix.h"
#include "splat_a.h"
#undef SPLAT_A
#undef SPLAT_B
  return 0;
}
