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

#include "random.h"
#include <math.h>

using namespace SubCritical;

static const struct ObjectMethod methods[] = {
  METHOD("Random", &RNG::Lua_Random),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(RNG, Object, methods);

uint32_t RNG::Random32() throw() {
  return (uint32_t)floor(RandomD() * 4294967296.0);
}

double RNG::RandomD() throw() {
  return Random32() * (1.0 / 4294967296.0);
}

int RNG::Lua_Random(lua_State* L) throw() {
  switch(lua_gettop(L)) {
  case 0:
    lua_pushnumber(L, RandomD());
    break;
  case 1:
    lua_pushnumber(L, Random32() % (uint32_t)luaL_checkinteger(L, 1) + 1);
    break;
  case 2:
    {
      uint32_t bot = luaL_checkinteger(L, 1), top = luaL_checkinteger(L, 2);
      lua_pushnumber(L, Random32() % (top-bot+1) + bot);
      break;
    }
  default:
    return luaL_error(L, "usage: Random([m [, n]])");
  }
  return 1;
}
