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

int GraphicLoader::Lua_Load(lua_State* L) throw() {
  int top = lua_gettop(L);
  if(top > 1) {
    static bool warned = false;
    if(!warned) {
      fprintf(stderr, "This game loads multiple graphics with the same Load call. This feature is deprecated and will be removed in a future version.\n");
      warned = true;
    }
    for(int n = 1; n <= top; ++n) {
      Load(GetPath(L, n))->Push(L);
    }
    return top;
  }
  else {
    const char* path = GetPath(L, 1);
    Graphic* ret = Load(path);
    if(!ret) {
      lua_pushnil(L);
      lua_pushfstring(L, "Unable to load graphic: %s", path);
      return 2;
    }
    else {
      ret->Push(L);
      return 1;
    }
  }
}

static const struct ObjectMethod GLMethods[] = {
  METHOD("Load", &GraphicLoader::Lua_Load),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(GraphicLoader, Object, GLMethods);
