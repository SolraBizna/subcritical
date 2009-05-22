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

#include "subcritical/graphics.h"

using namespace SubCritical;

class FakeDrawable : public Drawable {
public:
  PROTOCOL_PROTOTYPE();
  FakeDrawable(Drawable*restrict src, int x, int y, int w, int h);
  virtual ~FakeDrawable();
};

FakeDrawable::FakeDrawable(Drawable*restrict src, int x, int y, int w, int h) {
  this->width = w;
  this->height = h;
  this->has_alpha = src->has_alpha;
  this->simple_alpha = src->simple_alpha;
  this->fake_alpha = src->fake_alpha;
  this->layout = src->layout;
  if(src->rows[1] > src->rows[0])
    SetupDrawable(src->buffer + x + y * (src->rows[1] - src->rows[0]), src->rows[1] - src->rows[0]);
  else
    SetupDrawable(src->buffer + x + (src->height - y - 1) * (src->rows[1] - src->rows[0]), src->rows[1] - src->rows[0]);
}

FakeDrawable::~FakeDrawable() {}

PROTOCOL_IMP_PLAIN(FakeDrawable, Drawable);

#define COOKIE ((void*)(Construct_FakeDrawable))

SUBCRITICAL_CONSTRUCTOR(FakeDrawable)(lua_State* L) {
  Drawable*restrict src = lua_toobject(L, 1, Drawable);
  int x, y, w, h;
  x = luaL_optinteger(L, 2, 0);
  y = luaL_optinteger(L, 3, 0);
  w = luaL_optinteger(L, 4, src->width);
  h = luaL_optinteger(L, 5, src->height);
  if(x < 0 || x >= src->width) return luaL_error(L, "x coordinate outside the source drawable");
  if(y < 0 || y >= src->height) return luaL_error(L, "y coordinate outside the source drawable");
  if(w <= 0 || x+w > src->width) return luaL_error(L, "bad width given source drawable");
  if(h <= 0 || y+h > src->height) return luaL_error(L, "bad height given source drawable");
  FakeDrawable*restrict ret = new FakeDrawable(src, x, y, w, h);
  if(!ret) return luaL_error(L, "bad puppy!");
  ret->Push(L);
  lua_pushlightuserdata(L, COOKIE);
  lua_gettable(L, LUA_REGISTRYINDEX);
  if(lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_pushlightuserdata(L, COOKIE);
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
  lua_pushvalue(L, -2);
  lua_pushvalue(L, 1);
  lua_settable(L, -3);
  lua_pop(L, 1);
  return 1;
}