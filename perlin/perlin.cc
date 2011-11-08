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

#include <math.h>

static const int p[512] = {
  /* The integers 0--255, randomly redistributed. */
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
  /* The integers 0--255 in the same order again. */
  151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
};

/* This is basically Ken Perlin's reference implementation, trivially modified
   for style and language. */
static inline double fade(double t) { return t*t*t*(t*(t*6-15)+10); }
static inline double lerp(double t, double a, double b) { return a+t*(b-a); }
static double grad(int hash, double x, double y, double z) {
  /* Convert the low 4 bits of the hash code into 12 gradient directions. */
  int h = hash & 15;
  double u = h<8 ? x : y,
    v = h<4 ? y : h==12||h==14 ? x : z;
  return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}
static double noise(double x, double y, double z) {
  /* Find the unit cube that contains the point. */
  int X = (int)floor(x) & 255;
  int Y = (int)floor(y) & 255;
  int Z = (int)floor(z) & 255;
  /* Find the relative {x,y,z} of the point in the cube. */
  x -= floor(x);
  y -= floor(y);
  z -= floor(z);
  /* Compute fade curves for each of {x,y,z}. */
  double u = fade(x), v = fade(y), w = fade(z);
  /* Hash the coordinates of the 8 cube corners. */
  int A = p[X  ]+Y, AA = p[A]+Z, AB = p[A+1]+Z;
  int B = p[X+1]+Y, BA = p[B]+Z, BB = p[B+1]+Z;
  /* Add blended results from the 8 corners of the cube. */
  return lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z),
                              grad(p[BA], x-1, y, z)),
                      lerp(u, grad(p[AB], x, y-1, z),
                           grad(p[BB], x-1, y-1, z))),
              lerp(v, lerp(u, grad(p[AA+1], x, y, z-1),
                           grad(p[BA+1], x-1, y, z-1)),
                   lerp(u, grad(p[AB+1], x, y-1, z-1),
                        grad(p[BB+1], x-1, y-1, z-1))));
}

SUBCRITICAL_UTILITY(PerlinNoise)(lua_State* L) {
  double x, y, z;
  x = luaL_checknumber(L, 1);
  y = luaL_checknumber(L, 2);
  z = luaL_optnumber(L, 3, 0.5);
  lua_pushnumber(L, noise(x,y,z));
  return 1;
}
