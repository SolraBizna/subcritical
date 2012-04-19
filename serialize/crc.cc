/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2012 Solra Bizna.

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
#include "serialize.h"

using namespace SubCritical;

static uint32_t crc_table[256];

// The following two functions were cribbed from RFC 1952. The code in question
// seems to be public domain.
LOCAL void SubCritical::build_crc_table() {
  for(int n = 0; n < 256; ++n) {
    uint32_t c = n;
    for(int k = 0; k < 8; ++k) {
      if(c & 1)
	c = 0xedb88320L ^ (c >> 1);
      else
	c = c >> 1;
    }
    crc_table[n] = c;
  }
}

static uint32_t update_crc(uint32_t crc, const unsigned char* buf, size_t len) {
  crc = ~crc;
  UNROLL_MORE(len,
	      crc = crc_table[(crc ^ *buf++) & 255] ^ (crc >> 8));
  return ~crc;
}

SUBCRITICAL_UTILITY(QuickChecksum)(lua_State* L) {
  int top = lua_gettop(L);
  for(int n = 1; n <= top; ++n) {
    size_t len;
    const unsigned char* str = (const unsigned char*)luaL_checklstring(L, n, &len);
    uint32_t crc = update_crc(0, str, len);
    unsigned char buf[4] = {crc >> 24, crc >> 16, crc >> 8, crc};
    lua_pushlstring(L, (char*)buf, 4);
  }
  return top;
}

SUBCRITICAL_UTILITY(QuickChecksumHex)(lua_State* L) {
  int top = lua_gettop(L);
  for(int n = 1; n <= top; ++n) {
    size_t len;
    const unsigned char* str = (const unsigned char*)luaL_checklstring(L, n, &len);
    uint32_t crc = update_crc(0, str, len);
    char buf[9];
    snprintf(buf, 9, "%08X", crc);
    lua_pushlstring(L, buf, 8);
  }
  return top;
}
