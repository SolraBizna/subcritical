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

// include everything the SFMT code includes so it doesn't get included inside
// our super-struct
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define assert(p) ((void)(p)) // help work around a dumb bug in Cygwin's libc

#ifdef HAVE_ALTIVEC
#include <altivec.h>
#endif
#ifdef HAVE_SSE2
#include <emmintrin.h>
#endif

struct LOCAL SFMT19937_int {
#define MEXP 19937
#define static // this allows us to do the below with the minimum changes possible
#include "SFMT.c"
#undef static
};

using namespace SubCritical;

class SFMT19937 : public RNG {
public:
  PROTOCOL_PROTOTYPE();
  SFMT19937(uint32_t*, int);
  virtual double RandomD() throw();
  virtual uint32_t Random32() throw();
private:
  SFMT19937_int sfmt;
};

double SFMT19937::RandomD() throw() {
  return sfmt.genrand_res53();
}

uint32_t SFMT19937::Random32() throw() {
  return sfmt.gen_rand32();
}

SFMT19937::SFMT19937(uint32_t* seeds, int seed_count) {
  /* stuff that was initialized directly in SFMT.c but can't be because that
     is now a giant struct declaration */
  sfmt.psfmt32 = &sfmt.sfmt[0].u[0];
#if !defined(BIG_ENDIAN64) || defined(ONLY64)
  sfmt.psfmt64 = (uint64_t*)&sfmt.sfmt[0].u[0];
#endif
  sfmt.initialized = 0;
  sfmt.parity[0] = PARITY1;
  sfmt.parity[1] = PARITY2;
  sfmt.parity[2] = PARITY3;
  sfmt.parity[3] = PARITY4;
  /* end stuff */
  sfmt.init_by_array(seeds, seed_count);
}

PROTOCOL_IMP_PLAIN(SFMT19937, RNG);

SUBCRITICAL_CONSTRUCTOR(SFMT19937)(lua_State* L) {
  int count = lua_gettop(L);
  if(count == 0) return luaL_error(L, "at least one random seed must be provided");
  uint32_t seeds[count];
  for(int n = 0; n < count; ++n) {
    seeds[n] = luaL_checkinteger(L, n+1);
  }
  (new SFMT19937(seeds, count))->Push(L);
  return 1;
}
