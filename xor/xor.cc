/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2011 Solra Bizna.

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

#include <stdlib.h>
#include <string.h>

SUBCRITICAL_UTILITY(XorString)(lua_State* L) {
  size_t l;
  const char* a = luaL_checklstring(L,1,&l);
  int b = luaL_checkinteger(L,2);
  char* c = (char*)malloc(l);
  char* p = c;
  size_t rem = l;
  UNROLL(rem, *p++ = *a++ ^ b;);
  lua_pushlstring(L,c,l);
  free(c);
  return 1;
}

SUBCRITICAL_UTILITY(XorStrings)(lua_State* L) {
  size_t l1, l2;
  const char* a = luaL_checklstring(L,1,&l1);
  const char* b = luaL_checklstring(L,2,&l2);
  const char* lon;
  size_t minsize, maxsize;
  if(l1 > l2) { lon = a; maxsize=l1; minsize=l2; }
  else { lon = b; maxsize=l2; minsize=l1; }
  char* c = (char*)malloc(maxsize);
  memcpy(c+minsize,lon+minsize,maxsize-minsize);
  char* p = c;
  size_t rem = minsize;
  UNROLL(rem, *p++ = *a++ ^ *b++;);
  lua_pushlstring(L,c,maxsize);
  free(c);
  return 1;
}
