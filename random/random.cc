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
    {
      double n = (double)luaL_checkinteger(L, 1);
      if(n <= 1) lua_pushnumber(L, 1);
      else lua_pushnumber(L, Random32() % (uint32_t)n + 1);
    }
    break;
  case 2:
    {
      double bot = (double)luaL_checkinteger(L, 1), top = (double)luaL_checkinteger(L, 2);
      if(top < bot) {
	double x;
	x = bot; bot = top; top = x;
      }
      top = top + 1;
      double range=top-bot;
      if(range <= 1) lua_pushnumber(L, bot);
      else lua_pushnumber(L, fmod((double)Random32(), range) + (double)bot);
      break;
    }
  default:
    return luaL_error(L, "usage: Random([m [, n]])");
  }
  return 1;
}
