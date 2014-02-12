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
#include "serialize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace SubCritical;

LOCAL bool SubCritical::float_little_endian;

static struct canon {
  double raw;
  unsigned char expected[8];
} canons[] = {
  {(double)64,      {0x40,0x50,0x00,0x00,0x00,0x00,0x00,0x00}},
  {(double)-1.5,    {0xBF,0xF8,0x00,0x00,0x00,0x00,0x00,0x00}},
  {(double)0.15625, {0x3F,0xC4,0x00,0x00,0x00,0x00,0x00,0x00}},
};
static const int num_canons = sizeof(canons)/sizeof(*canons);

LUA_EXPORT int Init_serialize(lua_State* L) {
  build_crc_table();
#if 0
  for(int i = 0; i < num_canons; ++i) {
    union { double d; unsigned char u[sizeof(double)]; } bleh;
    bleh.d = canons[i].raw;
    for(int j = 0; j < (int)sizeof(double); ++j) {
      printf("%02X", bleh.u[j]);
    }
    putchar('\n');
  }
#endif
  if(sizeof(double) != 8)
    fprintf(stderr, "WARNING: Your compiler's double is not 64-bit! Celduins serialized on this computer will likely fail catastrophically on others!\n");
  else {
    bool ok = true;
    for(int i = 0; i < num_canons; ++i) {
      if(memcmp(&canons[i].raw, canons[i].expected, 8)) {
	ok = false;
	break;
      }
    }
    if(ok) float_little_endian = false;
    else {
      for(int i = 0; i < num_canons; ++i) {
	for(int x = 0; x < 4; ++x) {
	  canons[i].expected[x] ^= canons[i].expected[7-x];
	  canons[i].expected[7-x] ^= canons[i].expected[x];
	  canons[i].expected[x] ^= canons[i].expected[7-x];
	}
      }
      ok = true;
      for(int i = 0; i < num_canons; ++i) {
	if(memcmp(&canons[i].raw, canons[i].expected, 8)) {
	  ok = false;
	  break;
	}
      }
      if(ok)
	float_little_endian = true;
      else {
	fprintf(stderr, "WARNING: Your compiler's doubles are not IEEE 754-1985 doubles! Celduins serialized on this computer will likely not be compatible with others!\n");
	float_little_endian = false; // don't swap
      }
    }
  }
  return 0;
}
