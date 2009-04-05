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

#include "graphics.h"

using namespace SubCritical;

#define double_area_triangle(a,b,c) \
  (((a)[0]*(b)[1] - (b)[0]*(a)[1]) + ((b)[0]*(c)[1] - (c)[0]*(b)[1]) + ((c)[0]*(a)[1] - (a)[0]*(c)[1]))
#define cull_triangle(a,b,c,dir) \
  (double_area_triangle(a,b,c) dir 0)

SUBCRITICAL_UTILITY(CullTrianglesCW)(lua_State* L) {
  const CoordArray* coords = lua_toobject(L, 1, CoordArray);
  const IndexArray* indices = lua_toobject(L, 2, IndexArray);
  IndexArray* ret = new IndexArray(indices->count);
  size_t in, out;
  for(in = 0, out = 0; in < indices->count-2; in += 3) {
    if(cull_triangle(coords->coords + indices->indices[in]*2,
		     coords->coords + indices->indices[in+1]*2,
		     coords->coords + indices->indices[in+2]*2, >)) {
      ret->indices[out++] = indices->indices[in];
      ret->indices[out++] = indices->indices[in+1];
      ret->indices[out++] = indices->indices[in+2];
    }
  }
  if(out == 0) {
    delete ret;
    lua_pushnil(L);
  }
  else {
    ret->count = out;
    ret->Push(L);
  }
  return 1;
}

SUBCRITICAL_UTILITY(CullTrianglesCCW)(lua_State* L) {
  const CoordArray* coords = lua_toobject(L, 1, CoordArray);
  const IndexArray* indices = lua_toobject(L, 2, IndexArray);
  IndexArray* ret = new IndexArray(indices->count);
  size_t in, out;
  for(in = 0, out = 0; in < indices->count-2; in += 3) {
    if(cull_triangle(coords->coords + indices->indices[in]*2,
		     coords->coords + indices->indices[in+1]*2,
		     coords->coords + indices->indices[in+2]*2, <)) {
      ret->indices[out++] = indices->indices[in];
      ret->indices[out++] = indices->indices[in+1];
      ret->indices[out++] = indices->indices[in+2];
    }
  }
  if(out == 0) {
    delete ret;
    lua_pushnil(L);
  }
  else {
    ret->count = out;
    ret->Push(L);
  }
  return 1;
}
