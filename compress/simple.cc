/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2011-2013 Solra Bizna.

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
#include "subcritical/core.h"

#include <zlib.h>
#include <stdlib.h>
#include <lauxlib.h>

using namespace SubCritical;

SUBCRITICAL_UTILITY(Compress)(lua_State* L) throw() {
  size_t len;
  const char* string = luaL_checklstring(L,1,&len);
  int level = luaL_optinteger(L,2,6);
  if(level < 1 || level > 9)
    return luaL_error(L, "level must be between 1 and 9 inclusive");
  uLongf dstlen = compressBound(len);
  char* buf = (char*)malloc(dstlen);
  if(!buf)
    return luaL_error(L, "memory allocation error");
  if(compress2((Bytef*)buf,&dstlen,(const Bytef*)string,len,level) != Z_OK) {
    free(buf);
    return luaL_error(L, "compress2 failed.");
  }
  lua_pushlstring(L, buf, dstlen);
  free(buf);
  return 1;
}

SUBCRITICAL_UTILITY(Uncompress)(lua_State* L) throw() {
  size_t len;
  const char* string = luaL_checklstring(L,1,&len);
  uLongf dstlen = luaL_checkinteger(L,2);
  char* buf = (char*)malloc(dstlen);
  if(!buf)
    return luaL_error(L, "memory allocation error");
  if(uncompress((Bytef*)buf,&dstlen,(const Bytef*)string,len) != Z_OK) {
    free(buf);
    return luaL_error(L, "uncompress failed. (bad stream? wrong length?)");
  }
  lua_pushlstring(L, buf, dstlen);
  free(buf);
  return 1;
}
