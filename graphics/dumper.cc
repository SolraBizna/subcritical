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
#include "graphics.h"

using namespace SubCritical;

DumpOut::DumpOut(lua_State* L) : L(L) {
}

bool DumpOut::Write(const void* ptr, unsigned long size) {
  lua_pushvalue(L, 3);
  lua_pushvalue(L, 2);
  lua_pushlstring(L, (const char*)ptr, size);
  lua_call(L,2,1);
  bool ret = lua_toboolean(L,-1);
  lua_pop(L,1);
  return ret;
}

int GraphicDumper::Lua_Dump(lua_State* L) throw() {
  Graphic* graphic = lua_toobject(L, 1, Graphic);
  if(lua_gettop(L) < 3 || !lua_isfunction(L, 3))
    return luaL_error(L, "You must provide a value and the function to call on it. (for example, Dump(..., file, file.write))");
  bool success = false;
  const char* err = NULL;
  try {
    DumpOut out(L);
    success = Dump(graphic, out, err); 
  }
  catch(const char* e) {
    err = e;
  }
  if(!success) {
    lua_pushboolean(L, 0);
    if(err)
      lua_pushstring(L, err);
    else
      lua_pushliteral(L, "Unknown error");
    return 2;
  }
  else {
    lua_pushboolean(L, 1);
    return 1;
  }
}

static const struct ObjectMethod GDMethods[] = {
  METHOD("Dump", &GraphicDumper::Lua_Dump),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(GraphicDumper, Object, GDMethods);
