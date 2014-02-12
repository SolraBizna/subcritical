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

#include "subcritical/graphics.h"

#include <math.h>

using namespace SubCritical;

static Pixel num2lin(lua_Number n) {
  int32_t lin = (int32_t)floorf(n * 255.f + 0.5f);
  if(lin < 0) return 0;
  else if(lin > 255) return 255;
  else return lin;
}

static Pixel num2srgb(lua_Number n) {
  int32_t lin = (int32_t)floorf(n * 65535.f + 0.5f);
  if(lin < 0) return 0;
  else if(lin > 65535) return 255;
  else return LinearToSrgb[lin];
}

SUBCRITICAL_UTILITY(Render)(lua_State* L) {
  if(!lua_isfunction(L, 1)) return luaL_typerror(L, 1, "function");
  Graphic* ret = new Graphic(luaL_checkinteger(L, 2), luaL_checkinteger(L, 3), FB_xRGB);
  bool alpha = lua_toboolean(L, 4);
  ret->Push(L); // we want it to be collected on error, this is the easiest way
  if(alpha) {
    for(int y = 0; y < ret->height; ++y) {
      Pixel* dest = ret->rows[y];
      for(int x = 0; x < ret->width; ++x) {
	lua_pushvalue(L, 1);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_call(L, 2, 4);
	*dest++ = (num2lin(lua_tonumber(L,-1)) << 24) | (num2srgb(lua_tonumber(L,-4)) << 16) | (num2srgb(lua_tonumber(L,-3)) << 8) | num2srgb(lua_tonumber(L,-2));
	lua_pop(L, 4);
      }
    }
    ret->CheckAlpha();
  }
  else {
    Pixel mask = 0xFF000000;
    ret->has_alpha = false;
    for(int y = 0; y < ret->height; ++y) {
      Pixel* dest = ret->rows[y];
      for(int x = 0; x < ret->width; ++x) {
	lua_pushvalue(L, 1);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_call(L, 2, 3);
	*dest++ = mask | (num2srgb(lua_tonumber(L,-3)) << 16) | (num2srgb(lua_tonumber(L,-2)) << 8) | num2srgb(lua_tonumber(L,-1));
	lua_pop(L, 3);
      }
    }
  }
  return 1;
}

SUBCRITICAL_UTILITY(RenderPreCompressed)(lua_State* L) {
  if(!lua_isfunction(L, 1)) return luaL_typerror(L, 1, "function");
  Graphic* ret = new Graphic(luaL_checkinteger(L, 2), luaL_checkinteger(L, 3), FB_xRGB);
  bool alpha = lua_toboolean(L, 4);
  ret->Push(L); // we want it to be collected on error, this is the easiest way
  if(alpha) {
    for(int y = 0; y < ret->height; ++y) {
      Pixel* dest = ret->rows[y];
      for(int x = 0; x < ret->width; ++x) {
	lua_pushvalue(L, 1);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_call(L, 2, 4);
	*dest++ = (num2lin(lua_tonumber(L,-1)) << 24) | (num2lin(lua_tonumber(L,-4)) << 16) | (num2lin(lua_tonumber(L,-3)) << 8) | num2lin(lua_tonumber(L,-2));
	lua_pop(L, 4);
      }
    }
    ret->CheckAlpha();
  }
  else {
    Pixel mask = 0xFF000000;
    ret->has_alpha = false;
    for(int y = 0; y < ret->height; ++y) {
      Pixel* dest = ret->rows[y];
      for(int x = 0; x < ret->width; ++x) {
	lua_pushvalue(L, 1);
	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_call(L, 2, 3);
	*dest++ = mask | (num2lin(lua_tonumber(L,-3)) << 16) | (num2lin(lua_tonumber(L,-2)) << 8) | num2lin(lua_tonumber(L,-1));
	lua_pop(L, 3);
      }
    }
  }
  return 1;
}

SUBCRITICAL_UTILITY(RenderFrisket)(lua_State* L) {
  if(!lua_isfunction(L, 1)) return luaL_typerror(L, 1, "function");
  Frisket* ret = new Frisket(luaL_checkinteger(L, 2), luaL_checkinteger(L, 3));
  ret->Push(L); // we want it to be collected on error, this is the easiest way
  for(int y = 0; y < ret->height; ++y) {
    Frixel* dest = ret->rows[y];
    for(int x = 0; x < ret->width; ++x) {
      lua_pushvalue(L, 1);
      lua_pushnumber(L, x);
      lua_pushnumber(L, y);
      lua_call(L, 2, 1);
      *dest++ = num2lin(lua_tonumber(L,-1));
      lua_pop(L, 1);
    }
  }
  return 1;
}
