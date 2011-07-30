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
#include "subcritical/core.h"
#include <assert.h>
#include <string.h>

using namespace SubCritical;

// this is a modified version of my old Lua SHA-256 module.

#if defined(__powerpc__) || defined(__ppc__)
#define POWERPC 1
#endif

#if POWERPC
#define rotateleft(operand, constant) \
  ({uint32_t __rot; \
    __asm__("rlwinm %0,%1," #constant ",0xFFFFFFFF" : "=r"(__rot) : "r"(operand)); \
    __rot;})
#else
static uint32_t rotateleft(uint32_t operand, int constant) {
  return (operand << constant) | (operand >> (32 - constant));
}
#endif

static uint32_t k[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

class SixtyFour {
public:
  SixtyFour(const unsigned char* buf, size_t len) : buf(buf), rem(len), superp(superbuf) {
    superbuf[0] = 0x80;
    remsuperbuf = 1;
    while((len + remsuperbuf) % 64 != 56) superbuf[remsuperbuf++] = 0;
    uint64_t biglen = len;
    superbuf[remsuperbuf++] = biglen >> 56;
    superbuf[remsuperbuf++] = biglen >> 48;
    superbuf[remsuperbuf++] = biglen >> 40;
    superbuf[remsuperbuf++] = biglen >> 32;
    superbuf[remsuperbuf++] = biglen >> 24;
    superbuf[remsuperbuf++] = biglen >> 16;
    superbuf[remsuperbuf++] = biglen >> 8;
    superbuf[remsuperbuf++] = biglen;
  }
  bool Next64(unsigned char out[64]) {
    size_t n = 64;
    if(rem) {
      if(rem < n) {
	memcpy(out, buf, rem);
	out = out + rem;
	n -= rem;
	rem = 0;
	// fall through
      }
      else {
	memcpy(out, buf, n);
	rem -= n;
	return false;
      }
    }
    assert(remsuperbuf);
    assert(remsuperbuf >= n);
    memcpy(out, superp, n);
    remsuperbuf -= n;
    superp += n;
    return remsuperbuf == 0;
  }
private:
  const unsigned char* buf;
  size_t rem;
  unsigned char superbuf[73];
  unsigned char* superp;
  size_t remsuperbuf;
};

static void sha256(const unsigned char* message, size_t size, unsigned char digest[32]) {
  uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a,
    h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;
  uint32_t a, b, c, d, e, f, g, h;
  SixtyFour buf(message, size);
  bool eof;
  do {
    uint32_t w[64];
    {
      if(little_endian) {
        unsigned char x[64];
        unsigned char* p = x;
        eof = buf.Next64(x);
	w[0] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[1] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[2] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[3] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[4] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[5] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[6] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[7] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[8] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[9] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[10] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[11] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[12] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[13] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[14] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
	w[15] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; p += 4;
      }
      else {
        eof = buf.Next64((unsigned char*)w);
	/*w[0] = *(uint32_t*)p; p += 4;
	w[1] = *(uint32_t*)p; p += 4;
	w[2] = *(uint32_t*)p; p += 4;
	w[3] = *(uint32_t*)p; p += 4;
	w[4] = *(uint32_t*)p; p += 4;
	w[5] = *(uint32_t*)p; p += 4;
	w[6] = *(uint32_t*)p; p += 4;
	w[7] = *(uint32_t*)p; p += 4;
	w[8] = *(uint32_t*)p; p += 4;
	w[9] = *(uint32_t*)p; p += 4;
	w[10] = *(uint32_t*)p; p += 4;
	w[11] = *(uint32_t*)p; p += 4;
	w[12] = *(uint32_t*)p; p += 4;
	w[13] = *(uint32_t*)p; p += 4;
	w[14] = *(uint32_t*)p; p += 4;
	w[15] = *(uint32_t*)p; p += 4;*/
      }
    }
    for(int i = 16; i < 64; ++i) {
      w[i] = w[i-16] + (rotateleft(w[i-15], 25) ^ rotateleft(w[i-15], 14) ^ (w[i-15] >> 3)) + w[i-7] + (rotateleft(w[i-2], 15) ^ rotateleft(w[i-2], 13) ^ (w[i-2] >> 10));
    }
    a = h0; b = h1; c = h2; d = h3; e = h4; f = h5; g = h6; h = h7;
    for(int i = 0; i < 64; ++i) {
      uint32_t s0, maj, s1, ch, t1;
      s0 = rotateleft(a, 30) ^ rotateleft(a, 19) ^ rotateleft(a, 10);
      maj = (a & b) ^ (a & c) ^ (b & c);
      s1 = rotateleft(e, 26) ^ rotateleft(e, 21) ^ rotateleft(e, 7);
      ch = (e & f) ^ (~e & g);
      t1 = h + s1 + ch + k[i] + w[i];
      h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + s0 + maj;
    }
    h0 += a; h1 += b; h2 += c; h3 += d; h4 += e; h5 += f; h6 += g; h7 += h;
  } while(!eof);
  if(little_endian) {
    digest[ 0] = h0 >> 24; digest[ 1] = h0 >> 16; digest[ 2] = h0 >> 8; digest[ 3] = h0;
    digest[ 4] = h1 >> 24; digest[ 5] = h1 >> 16; digest[ 6] = h1 >> 8; digest[ 7] = h1;
    digest[ 8] = h2 >> 24; digest[ 9] = h2 >> 16; digest[10] = h2 >> 8; digest[11] = h2;
    digest[12] = h3 >> 24; digest[13] = h3 >> 16; digest[14] = h3 >> 8; digest[15] = h3;
    digest[16] = h4 >> 24; digest[17] = h4 >> 16; digest[18] = h4 >> 8; digest[19] = h4;
    digest[20] = h5 >> 24; digest[21] = h5 >> 16; digest[22] = h5 >> 8; digest[23] = h5;
    digest[24] = h6 >> 24; digest[25] = h6 >> 16; digest[26] = h6 >> 8; digest[27] = h6;
    digest[28] = h7 >> 24; digest[29] = h7 >> 16; digest[30] = h7 >> 8; digest[31] = h7;
  }
  else {
    ((uint32_t*)digest)[0] = h0;
    ((uint32_t*)digest)[1] = h1;
    ((uint32_t*)digest)[2] = h2;
    ((uint32_t*)digest)[3] = h3;
    ((uint32_t*)digest)[4] = h4;
    ((uint32_t*)digest)[5] = h5;
    ((uint32_t*)digest)[6] = h6;
    ((uint32_t*)digest)[7] = h7;
  }
}

SUBCRITICAL_UTILITY(GoodChecksum)(lua_State* L) {
  int top = lua_gettop(L);
  for(int n = 1; n <= top; ++n) {
    size_t len;
    const unsigned char* str = (const unsigned char*)luaL_checklstring(L, n, &len);
    unsigned char digest[32];
    sha256(str, len, digest);
    lua_pushlstring(L, (char*)digest, 32);
  }
  return top;
}

static const char digits[17] = "0123456789ABCDEF";

SUBCRITICAL_UTILITY(GoodChecksumHex)(lua_State* L) {
  int top = lua_gettop(L);
  for(int n = 1; n <= top; ++n) {
    size_t len;
    const unsigned char* str = (const unsigned char*)luaL_checklstring(L, n, &len);
    unsigned char digest[32];
    sha256(str, len, digest);
    lua_pushlstring(L, (char*)digest, 32);
    char buf[64];
    for(int i = 0; i < 32; ++i) {
      buf[i*2] = digits[digest[i]>>4];
      buf[i*2+1] = digits[digest[i]&15];
    }
    lua_pushlstring(L, buf, 64);
  }
  return top;
}
