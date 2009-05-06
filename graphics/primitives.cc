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
#include "graphics.h"

#include <math.h>
#include <string.h>

using namespace SubCritical;

static inline uint8_t linear_F_to_nonlinear_8(float f) {
  if(f >= 1.f) return 255;
  else if(f <= 0.f) return 0;
  else if(f < 0.0031308f) return (uint8_t)floorf(255.f * (12.92f * f) + 0.5);
  else return (uint8_t)floorf(255.f * (1.055f * powf(f, 0.41666) - 0.055) + 0.5);
}

static inline uint16_t linear_F_to_linear_16(float f) {
  return (uint16_t)floorf(65535.f * f + 0.5f);
}

// This DOES round correctly, trust me
static inline uint16_t linear_F_to_linear_16a(float f) {
  return (uint16_t)floorf(65536.f * f);
}

static inline uint16_t linear_F_to_linear_16p(float f, float a) {
  return (uint16_t)floorf(65535.f * (f*a) + 0.5f);
}

#define STD_PRIM *p++ = op_p
#define STD_PRIM_ALPHA r = (((uint32_t)SrgbToLinear[(*p >> rsh) & 255] * tr_a >> 16) + tr_r); g = (((uint32_t)SrgbToLinear[(*p >> gsh) & 255] * tr_a >> 16) + tr_g); b = (((uint32_t)SrgbToLinear[(*p >> bsh) & 255] * tr_a >> 16) + tr_b); if(r > 65535) r = 65535; if(g > 65535) g = 65535; if(b > 65535) b = 65535; *p++ = (LinearToSrgb[r] << rsh) | (LinearToSrgb[g] << gsh) | (LinearToSrgb[b] << bsh)
#define STD_PRIM_UNROLL UNROLL_MORE(rem, STD_PRIM)
#define STD_PRIM_UNROLL_ALPHA do { uint32_t r, g, b; UNROLL(rem, STD_PRIM_ALPHA); } while(0)

void Drawable::DrawRect(int l, int t, int r, int b) throw() {
  if(l < clip_left) l = clip_left;
  if(r > clip_right) r = clip_right;
  if(t < clip_top) t = clip_top;
  if(b > clip_bottom) b = clip_bottom;
  if(r < l || b < t) return;
  if(primitive_alpha) {
    for(int y = t; y <= b; ++y) {
      int rem = r - l + 1;
      Pixel* p = rows[y] + l;
      STD_PRIM_UNROLL_ALPHA;
    }
  }
  else {
    for(int y = t; y <= b; ++y) {
      int rem = r - l + 1;
      Pixel* p = rows[y] + l;
      STD_PRIM_UNROLL;
    }
  }
}

int Drawable::Lua_DrawRect(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  int l, t, r, b;
  l = (int)nearbyint(luaL_checknumber(L, 1));
  t = (int)nearbyint(luaL_checknumber(L, 2));
  r = l + (int)nearbyint(luaL_checknumber(L, 3)) - 1;
  b = t + (int)nearbyint(luaL_checknumber(L, 4)) - 1;
  DrawRect(l, t, r, b);
  return 0;
}

void Drawable::DrawBox(int l, int t, int r, int b, int sz) throw() {
  DrawRect(l, t, r, t+sz-1);
  DrawRect(l, b-sz+1, r, b);
  DrawRect(l, t+sz, l+sz-1, b-sz);
  DrawRect(r-sz+1, t+sz, r, b-sz);
}

int Drawable::Lua_DrawBox(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  int l, t, r, b;
  l = (int)nearbyint(luaL_checknumber(L, 1));
  t = (int)nearbyint(luaL_checknumber(L, 2));
  r = l + (int)nearbyint(luaL_checknumber(L, 3)) - 1;
  b = t + (int)nearbyint(luaL_checknumber(L, 4)) - 1;
  DrawBox(l, t, r, b, luaL_optinteger(L, 5, 1));
  return 0;
}

void Drawable::DrawPoints(int size, const Fixed* coords, size_t pointcount) throw() {
  const Fixed* fp = coords;
  if(primitive_alpha && tr_a == 0) return;
  if(size <= 1) {
    if(primitive_alpha) {
      uint32_t r, g, b;
      for(size_t n = 0; n < pointcount; ++n) {
	int x, y;
	x = Q_TO_I(fp[0]);
	y = Q_TO_I(fp[1]);
	fp += 2;
	if(x < clip_left || x > clip_right || y < clip_top || y > clip_bottom) continue;
	Pixel* p = rows[y] + x;
	STD_PRIM_ALPHA;
      }
    }
    else {
      for(size_t n = 0; n < pointcount; ++n) {
	int x, y;
	x = Q_TO_I(fp[0]);
	y = Q_TO_I(fp[1]);
	fp += 2;
	if(x < clip_left || x > clip_right || y < clip_top || y > clip_bottom) continue;
	Pixel* p = rows[y] + x;
	STD_PRIM;
      }
    }
  }
  else {
    // this needs to be a bit adjusted, but not really important
    int tl = size / 2;
    int br = tl + (size & 1) - 1;
    for(size_t n = 0; n < pointcount; ++n) {
      int x, y;
      x = Q_CEIL(fp[0]);
      y = Q_CEIL(fp[1]);
      fp += 2;
      DrawRect(x-tl, y-tl, x+br, y+br);
    }
  }
}

int Drawable::Lua_DrawPoints(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  CoordArray* r = lua_toobject(L, 1, CoordArray);
  DrawPoints((int)luaL_optnumber(L, 2, 1), r->coords, r->count);
  return 0;
}

#define X(f,i) (f)[(i)*2]
#define Y(f,i) (f)[(i)*2+1]

void Drawable::SetPrimitiveColor(lua_Number r, lua_Number g, lua_Number b, lua_Number a) throw() {
  if(a >= 1.0) {
    int op_r, op_g, op_b;
    primitive_alpha = false;
    op_r = linear_F_to_nonlinear_8((float)r);
    op_g = linear_F_to_nonlinear_8((float)g);
    op_b = linear_F_to_nonlinear_8((float)b);
    trf_r = linear_F_to_linear_16((float)r);
    trf_g = linear_F_to_linear_16((float)g);
    trf_b = linear_F_to_linear_16((float)b);
    op_p = (op_r << rsh) | (op_g << gsh) | (op_b << bsh);
  }
  else if(a <= 0.0) {
    primitive_alpha = true;
    tr_a = tr_b = tr_g = tr_r = 0;
  }
  else {
    primitive_alpha = true;
    tr_r = linear_F_to_linear_16p((float)r, (float)a);
    tr_g = linear_F_to_linear_16p((float)g, (float)a);
    tr_b = linear_F_to_linear_16p((float)b, (float)a);
    trf_r = linear_F_to_linear_16((float)r);
    trf_g = linear_F_to_linear_16((float)g);
    trf_b = linear_F_to_linear_16((float)b);
    trf_a = linear_F_to_linear_16a((float)a);
    if(trf_a == 0) tr_a = 0;
    else tr_a = 65536 - trf_a;
  }
}

int Drawable::Lua_SetPrimitiveColor(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  lua_Number r, g, b, a;
  r = luaL_checknumber(L, 1);
  g = luaL_checknumber(L, 2);
  b = luaL_checknumber(L, 3);
  a = luaL_optnumber(L, 4, 1.0);
  SetPrimitiveColor(r, g, b, a);
  return 0;
}

void Drawable::BlitFrisket(const Frisket* frisket, int dx, int dy) throw() {
  BlitFrisketRect(frisket, 0, 0, frisket->width, frisket->height, dx, dy);
}

void Drawable::BlitFrisketRect(const Frisket* gfk, int sx, int sy, int sw, int sh, int dx, int dy) throw() {
  int sl, st, sr, sb;
  sl = sx;
  st = sy;
  sr = sx + sw - 1;
  sb = sy + sh - 1;
  if(dx < clip_left) { sl += clip_left - dx; dx = clip_left; }
  if(dy < clip_top) { st += clip_top - dy; dy = clip_top; }
  if(dx + (sr - sl) > clip_right) { sr += clip_right - (dx + (sr - sl)); }
  if(dy + (sb - st) > clip_bottom) { sb += clip_bottom - (dy + (sb - st)); }
  if(sl < 0) { dx -= sl; sl = 0; }
  if(sr >= gfk->width) sr = gfk->width - 1;
  if(st < 0) { dy -= st; st = 0; }
  if(sb >= gfk->height) sb = gfk->height - 1;
  if(sr < sl || sb < st) return;
  if(primitive_alpha) {
    if(tr_a == 0) return;
    for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
      Frixel* src;
      Pixel* dst;
      uint32_t r, g, b, a, ra;
      src = gfk->rows[sY] + sl;
      dst = rows[dY] + dx;
      size_t rem = sr - sl + 1;
      UNROLL(rem,
	     a = *src;
	     a += a & 1;
	     a = a * trf_a >> 8;
	     ra = 65536-a;
	     r = (trf_r * a + (uint32_t)SrgbToLinear[(*dst >> rsh) & 255] * ra) >> 16;
	     g = (trf_g * a + (uint32_t)SrgbToLinear[(*dst >> gsh) & 255] * ra) >> 16;
	     b = (trf_b * a + (uint32_t)SrgbToLinear[(*dst >> bsh) & 255] * ra) >> 16;
	     ++src;
	     *dst++ = ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh));
    }
  }
  else for(int sY = st, dY = dy; sY <= sb; ++sY, ++dY) {
    Frixel* src;
    Pixel* dst;
    uint32_t r, g, b, a, ra;
    src = gfk->rows[sY] + sl;
    dst = rows[dY] + dx;
    size_t rem = sr - sl + 1;
    UNROLL(rem,
	   a = *src;
	   a += a & 1;
	   ra = 256-a;
	   r = (trf_r * a + (uint32_t)SrgbToLinear[(*dst >> rsh) & 255] * ra) >> 8;
	   g = (trf_g * a + (uint32_t)SrgbToLinear[(*dst >> gsh) & 255] * ra) >> 8;
	   b = (trf_b * a + (uint32_t)SrgbToLinear[(*dst >> bsh) & 255] * ra) >> 8;
	   ++src;
	   *dst++ = ((Pixel)LinearToSrgb[r] << rsh) | ((Pixel)LinearToSrgb[g] << gsh) | ((Pixel)LinearToSrgb[b] << bsh));
  }
}

int Drawable::Lua_BlitFrisket(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  Frisket* frisket = lua_toobject(L,1,Frisket);
  switch(lua_gettop(L)) {
  case 3: BlitFrisket(frisket, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3)); return 0;
  case 7: BlitFrisketRect(frisket, (int)luaL_checknumber(L,2), (int)luaL_checknumber(L,3), (int)luaL_checknumber(L,4), (int)luaL_checknumber(L,5), (int)luaL_checknumber(L,6), (int)luaL_checknumber(L,7)); return 0;
  default:
    return luaL_error(L, "BlitFrisket takes 3 or 7 parameters\n");
  }
}

/*inline void Drawable::DrawSpan(int y, Fixed l, Fixed r) {
  if(y < clip_top || y > clip_bottom) return;
  l = Q_TO_I(l);
  r = Q_TO_I(r);
  if(l < clip_left) l = clip_left;
  if(r > clip_right + 1) r = clip_right + 1;
  if(r <= l) return;
  int rem = r - l;
  Pixel* p = rows[y] + l;
  STD_PRIM_UNROLL;
}

inline void Drawable::DrawSpanA(int y, Fixed l, Fixed r) {
  if(y < clip_top || y > clip_bottom) return;
  l = Q_TO_I(l);
  r = Q_TO_I(r);
  if(l < clip_left) l = clip_left;
  if(r > clip_right + 1) r = clip_right + 1;
  if(r <= l) return;
  int rem = r - l;
  Pixel* p = rows[y] + l;
  STD_PRIM_UNROLL_ALPHA;
  }*/

inline void Drawable::NoclipDrawSpan(int y, Fixed l, Fixed r) {
  l = Q_TO_I(l);
  r = Q_TO_I(r);
  if(r <= l) return;
  int rem = r - l;
  Pixel* p = rows[y] + l;
  STD_PRIM_UNROLL;
}

inline void Drawable::NoclipDrawSpanA(int y, Fixed l, Fixed r) {
  l = Q_TO_I(l);
  r = Q_TO_I(r);
  if(r <= l) return;
  int rem = r - l;
  Pixel* p = rows[y] + l;
  STD_PRIM_UNROLL_ALPHA;
}

#define Tri_DDA_Vars(side) \
  Fixed side, side##i, side##s, side##e, side##n, side##d, side##bign; \
  int side##f;

#define Tri_DDA_Init(side, top, bot, dir)		\
side = top[!dir]; \
side##bign = bot[!dir] - top[!dir]; \
side##f = ((top[dir] + 32) & 63);	\
side##d = bot[dir] - top[dir]; \
if(!side##d) side##d = 0x7FFFFFFF; \
side##n = side##bign << 6; \
if(side##n < 0) { \
  side##n = -side##n; \
  side##i = -1; \
  side##s = -(side##n / side##d); \
  side##n = side##n % side##d; \
  side##bign = -side##bign; \
} \
else { \
  side##i = 1; \
  side##s = side##n / side##d; \
  side##n = side##n % side##d; \
} \
if(side##f == 0) side##e = 0; \
else { \
  side##e = side##bign * (64 - side##f);	\
  while(side##e >= side##d) {			\
    side##e -= side##d; \
    side += side##i; \
  } \
}
#define Tri_DDA_Step(side) \
side += side##s; \
side##e += side##n; \
if(side##e >= side##d) { \
  side##e -= side##d; \
  side += side##i; \
}

inline void Drawable::DrawTriangleR(const Fixed* top, const Fixed* mid, const Fixed* bot) throw() {
  int remt = Q_TO_I(mid[1]) - Q_TO_I(top[1]);
  int remb = Q_TO_I(bot[1] - 1) - Q_TO_I(mid[1]);
  if(!remt && !remb) return;
  int y = Q_TO_I(top[1]);
  Tri_DDA_Vars(l);
  Tri_DDA_Init(l, top, bot, 1);
  if(remt) {
    Tri_DDA_Vars(t);
    Tri_DDA_Init(t, top, mid, 1);
    if(primitive_alpha) while(remt-- > 0) {
      NoclipDrawSpanA(y++, l, t);
      Tri_DDA_Step(l);
      Tri_DDA_Step(t);
    }
    else while(remt-- > 0) {
      NoclipDrawSpan(y++, l, t);
      Tri_DDA_Step(l);
      Tri_DDA_Step(t);
    }
  }
  if(remb) {
    Tri_DDA_Vars(b);
    Tri_DDA_Init(b, mid, bot, 1);
    if(primitive_alpha) while(remb-- > 0) {
      NoclipDrawSpanA(y++, l, b);
      Tri_DDA_Step(l);
      Tri_DDA_Step(b);
    }
    else while(remb-- > 0) {
      NoclipDrawSpan(y++, l, b);
      Tri_DDA_Step(l);
      Tri_DDA_Step(b);
    }
  }
}

inline void Drawable::DrawTriangleL(const Fixed* top, const Fixed* mid, const Fixed* bot) throw() {
  int remt = Q_TO_I(mid[1]) - Q_TO_I(top[1]);
  int remb = Q_TO_I(bot[1] - 1) - Q_TO_I(mid[1]);
  if(!remt && !remb) return;
  Tri_DDA_Vars(r);
  Tri_DDA_Init(r, top, bot, 1);
  int y = Q_TO_I(top[1]);
  if(remt) {
    Tri_DDA_Vars(t);
    Tri_DDA_Init(t, top, mid, 1);
    if(primitive_alpha) while(remt-- > 0) {
      NoclipDrawSpanA(y++, t, r);
      Tri_DDA_Step(r);
      Tri_DDA_Step(t);
    }
    else while(remt-- > 0) {
      NoclipDrawSpan(y++, t, r);
      Tri_DDA_Step(r);
      Tri_DDA_Step(t);
    }
  }
  if(remb) {
    Tri_DDA_Vars(b);
    Tri_DDA_Init(b, mid, bot, 1);
    if(primitive_alpha) while(remb-- > 0) {
      NoclipDrawSpanA(y++, b, r);
      Tri_DDA_Step(r);
      Tri_DDA_Step(b);
    }
    else while(remb-- > 0) {
      NoclipDrawSpan(y++, b, r);
      Tri_DDA_Step(r);
      Tri_DDA_Step(b);
    }
  }
}

inline void Drawable::DrawTriangle(const Fixed* a, const Fixed* b, const Fixed* c) throw() {
  const Fixed* top, *mid, *bot;
  // We need to sort the triangle from top to bottom.
  if(a[1] > b[1]) {
    // A>B
    if(a[1] > c[1]) {
      // A>B,C
      if(b[1] > c[1]) {
	// A>B>C
        top = c; mid = b; bot = a;
      }
      else {
	// A>C>B
	top = b; mid = c; bot = a;
      }
    }
    else {
      // C>A>B
      top = b; mid = a; bot = c;
    }
  }
  else {
    // B>A
    if(b[1] > c[1]) {
      // B>A,C
      if(a[1] > c[1]) {
	// B>A>C
	top = c; mid = a; bot = b;
      }
      else {
	// B>C>A
	top = a; mid = c; bot = b;
      }
    }
    else {
      // C>B>A
      top = a; mid = b; bot = c;
    }
  }
  // Next, we determine whether the middle vertex sticks out left or right
  Fixed max, fact;
  max = bot[1] - top[1];
  fact = mid[1] - top[1];
  Fixed mx = (top[0] * (max - fact) + bot[0] * fact) / max;
  if(mid[0] < mx) DrawTriangleL(top, mid, bot);
  else if(mid[0] > mx) DrawTriangleR(top, mid, bot);
  // else, infinitely thin triangle
}

#define CLIP_LINE(coord, clip, a, b, c) \
i = clip - a[coord]; \
q = b[coord] - a[coord]; \
if(coord == 0) c[0] = clip; \
else c[0] = a[0] + ((b[0]-a[0]) * i / q); \
if(coord == 1) c[1] = clip; \
else c[1] = a[1] + ((b[1]-a[1]) * i / q)

#define Clipf(name, coord, dir) \
static inline int Clip##name(Fixed clip, const Fixed* a, const Fixed* b, const Fixed* c, Fixed* d, Fixed* e, int dn, int en, int mia[3], int via[3]) { \
  Fixed i, q; \
  if(a[coord] dir clip) { \
    if(b[coord] dir clip) { \
      if(c[coord] dir clip) return -1; \
      CLIP_LINE(coord, clip, a, c, d); \
      CLIP_LINE(coord, clip, b, c, e); \
      mia[0] = dn; \
      mia[1] = en; \
      return 1; \
    } \
    else if(c[coord] dir clip) { \
      CLIP_LINE(coord, clip, a, b, d); \
      CLIP_LINE(coord, clip, c, b, e); \
      mia[0] = dn; \
      mia[2] = en; \
      return 1; \
    } \
    else { \
      CLIP_LINE(coord, clip, a, b, d); \
      CLIP_LINE(coord, clip, a, c, e); \
      via[0] = dn; \
      via[1] = mia[1]; \
      via[2] = mia[2]; \
      mia[0] = dn; \
      mia[1] = mia[2]; \
      mia[2] = en; \
      return 2; \
    } \
  } \
  else if(b[coord] dir clip) { \
    if(c[coord] dir clip) { \
      CLIP_LINE(coord, clip, a, b, d); \
      CLIP_LINE(coord, clip, a, c, e); \
      mia[1] = dn; \
      mia[2] = en; \
      return 1; \
    } \
    else { \
      CLIP_LINE(coord, clip, b, a, d); \
      CLIP_LINE(coord, clip, b, c, e); \
      via[0] = dn; \
      via[1] = mia[0]; \
      via[2] = mia[2]; \
      mia[0] = dn; \
      mia[1] = en; \
      /*mia[2] = mia[2];*/			\
      return 2; \
    } \
  } \
  else if(c[coord] dir clip) { \
    CLIP_LINE(coord, clip, c, a, d); \
    CLIP_LINE(coord, clip, c, b, e); \
    via[0] = dn; \
    via[1] = mia[0]; \
    via[2] = mia[1]; \
    mia[0] = dn; \
    mia[2] = mia[1]; \
    mia[1] = en; \
    return 2; \
  } \
  else return 0; \
}

Clipf(Left, 0, <);
Clipf(Right, 0, >);
Clipf(Up, 1, <);
Clipf(Down, 1, >);

inline void Drawable::ClipNDrawTriangle(const Fixed* a, const Fixed* b, const Fixed* c) throw() {
#define MAXCLIPVERTICES (3+(MAXCLIPTRIANGLES*2))
#define MAXCLIPTRIANGLES 16
  Fixed* vertices[MAXCLIPVERTICES] = {const_cast<Fixed*>(a), const_cast<Fixed*>(b), const_cast<Fixed*>(c)};
  Fixed nuvertices[MAXCLIPTRIANGLES*2][2];
  for(int n = 0; n < MAXCLIPTRIANGLES; ++n) {
    vertices[n*2+3] = nuvertices[n*2];
    vertices[n*2+4] = nuvertices[n*2+1];
  }
  int numvertices = 3;
  int triangles[MAXCLIPTRIANGLES][3] = {{0, 1, 2}};
  int numtriangles = 1;
  Fixed coord;
  coord = I_TO_Q(clip_left);
  for(int n = numtriangles-1; n >= 0; --n) {
    switch(ClipLeft(coord, vertices[triangles[n][0]], vertices[triangles[n][1]], vertices[triangles[n][2]], vertices[numvertices], vertices[numvertices+1], numvertices, numvertices+1, triangles[n], triangles[numtriangles])) {
    case -1:
      memmove(triangles[n], triangles[n+1], (numtriangles-n-1) * sizeof(*triangles));
      --numtriangles;
      break;
    case 0:
    default:
      break;
    case 2:
      numtriangles += 1;
    case 1:
      numvertices += 2;
      break;
    }
  }
  coord = I_TO_Q(clip_right)+63;
  for(int n = numtriangles-1; n >= 0; --n) {
    switch(ClipRight(coord, vertices[triangles[n][0]], vertices[triangles[n][1]], vertices[triangles[n][2]], vertices[numvertices], vertices[numvertices+1], numvertices, numvertices+1, triangles[n], triangles[numtriangles])) {
    case -1:
      memmove(triangles[n], triangles[n+1], (numtriangles-n-1) * sizeof(*triangles));
      --numtriangles;
      break;
    case 0:
    default:
      break;
    case 2:
      numtriangles += 1;
    case 1:
      numvertices += 2;
      break;
    }
  }
  coord = I_TO_Q(clip_bottom)+63;
  for(int n = numtriangles-1; n >= 0; --n) {
    int ot[3] = {triangles[n][0], triangles[n][1], triangles[n][2]};
    switch(ClipDown(coord, vertices[triangles[n][0]], vertices[triangles[n][1]], vertices[triangles[n][2]], vertices[numvertices], vertices[numvertices+1], numvertices, numvertices+1, triangles[n], triangles[numtriangles])) {
    case -1:
      memmove(triangles[n], triangles[n+1], (numtriangles-n-1) * sizeof(*triangles));
      --numtriangles;
      break;
    case 0:
    default:
      break;
    case 2:
      if(ot[0] != triangles[n][0] || ot[1] != triangles[n][1] || ot[2] != triangles[n][2])
      numtriangles += 1;
    case 1:
      numvertices += 2;
      break;
    }
  }
  coord = I_TO_Q(clip_top);
  for(int n = numtriangles-1; n >= 0; --n) {
    int ot[3] = {triangles[n][0], triangles[n][1], triangles[n][2]};
    switch(ClipUp(coord, vertices[triangles[n][0]], vertices[triangles[n][1]], vertices[triangles[n][2]], vertices[numvertices], vertices[numvertices+1], numvertices, numvertices+1, triangles[n], triangles[numtriangles])) {
    case -1:
      memmove(triangles[n], triangles[n+1], (numtriangles-n-1) * sizeof(*triangles));
      --numtriangles;
      break;
    case 0:
    default:
      break;
    case 2:
      if(ot[0] != triangles[n][0] || ot[1] != triangles[n][1] || ot[2] != triangles[n][2])
      numtriangles += 1;
    case 1:
      numvertices += 2;
      break;
    }
  }
  for(int n = 0; n < numtriangles; ++n) {
    DrawTriangle(vertices[triangles[n][0]],vertices[triangles[n][1]],vertices[triangles[n][2]]);
  }
}

inline void Drawable::DrawBresenlineV(const Fixed* top, const Fixed* bot) {
  if(top[1] > bot[1]) {
    const Fixed* t;
    t = top; top = bot; bot = t;
  }
  int rem = Q_TO_I(bot[1]) - Q_TO_I(top[1]);
  if(rem <= 0) return;
  int y = Q_TO_I(top[1]);
  Tri_DDA_Vars(line);
  Tri_DDA_Init(line, top, bot, 1);
  if(primitive_alpha) {
    uint32_t r, g, b;
    while((Q_TO_I(line) < 0 || Q_TO_I(line) >= width ||
	   y < 0 || y >= height) &&
	   rem-- > 0) {
      ++y;
      Tri_DDA_Step(line);
    }
    while(Q_TO_I(line) >= 0 && Q_TO_I(line) < width &&
	  y >= 0 && y < height &&
	  rem-- > 0) {
      Pixel* p = rows[y++] + Q_TO_I(line);
      STD_PRIM_ALPHA;
      Tri_DDA_Step(line);
    }
  }
  else {
    while((Q_TO_I(line) < 0 || Q_TO_I(line) >= width ||
	   y < 0 || y >= height) &&
	  rem-- > 0) {
      ++y;
      Tri_DDA_Step(line);
    }
    while(Q_TO_I(line) >= 0 && Q_TO_I(line) < width &&
	  y >= 0 && y < height &&
	  rem-- > 0) {
      Pixel* p = rows[y++] + Q_TO_I(line);
      STD_PRIM;
      Tri_DDA_Step(line);
    }
  }
}

inline void Drawable::DrawBresenlineH(const Fixed* left, const Fixed* right) {
  if(left[0] > right[0]) {
    const Fixed* t;
    t = left; left = right; right = t;
  }
  int rem = Q_TO_I(right[0]) - Q_TO_I(left[0]);
  if(rem <= 0) return;
  int x = Q_TO_I(left[0]);
  Tri_DDA_Vars(line);
  Tri_DDA_Init(line, left, right, 0);
  if(primitive_alpha) {
    uint32_t r, g, b;
    while((Q_TO_I(line) < 0 || Q_TO_I(line) >= height ||
	   x < 0 || x >= width) &&
	   rem-- > 0) {
      ++x;
      Tri_DDA_Step(line);
    }
    while(Q_TO_I(line) >= 0 && Q_TO_I(line) < height &&
	  x >= 0 && x < width &&
	  rem-- > 0) {
      Pixel* p = rows[Q_TO_I(line)] + x++;
      STD_PRIM_ALPHA;
      Tri_DDA_Step(line);
    }
  }
  else {
    while((Q_TO_I(line) < 0 || Q_TO_I(line) >= height ||
	   x < 0 || x >= width) &&
	   rem-- > 0) {
      ++x;
      Tri_DDA_Step(line);
    }
    while(Q_TO_I(line) >= 0 && Q_TO_I(line) < height &&
	  x >= 0 && x < width &&
	  rem-- > 0) {
      Pixel* p = rows[Q_TO_I(line)] + x++;
      STD_PRIM;
      Tri_DDA_Step(line);
    }
  }
}

inline void Drawable::DrawBresenline(const Fixed* a, const Fixed* b) {
  Fixed rise, run;
  rise = b[1] - a[1];
  run = b[0] - a[0];
  if(rise == 0 && run == 0) return;
  if(rise < 0) rise = -rise;
  if(run < 0) run = -run;
  if(rise > run) DrawBresenlineV(a, b);
  else DrawBresenlineH(a, b);
}

// Returns 0 instead of complex numbers.
inline static Fixed QuickFixedSqrt(signed long long n) {
  Fixed i = 32768, ib = 16384;
  // I can't believe I thought this would be fast enough!
  //for(i = 1; i < 65536 && (((long long)i*i)>>6) < n; ++i)
  //;
  do {
    if((((long long)i*i)>>6) >= n)
      i -= ib;
    else
      i += ib;
    ib = ib >> 1;
  } while(ib);
  return i - 1;
}

/* Removed to speed up compilation and simplify clipping */
/*inline void Drawable::DrawQuad(const Fixed* top, const Fixed* left, const Fixed* right, const Fixed* bot) {
  const Fixed* sw;
#define SWAP(a,b) do { sw = a; a = b; b = sw; } while(0)
#define Quad_StepDualSub(subsmall, sublarge) \
  while((subsmall)-- > 0) { \
    --(sublarge); --rem;    \
    DrawSpan(y++, l, r); \
    Tri_DDA_Step(l); \
    Tri_DDA_Step(r); \
  }
#define Quad_StepSingleSub(sub) \
  while((sub)-- > 0) {		\
    --rem; \
    DrawSpan(y++, l, r); \
    Tri_DDA_Step(l); \
    Tri_DDA_Step(r); \
  }
#define Quad_StepSub() \
  while(rem-- > 0) { \
    DrawSpan(y++, l, r); \
    Tri_DDA_Step(l); \
    Tri_DDA_Step(r); \
  }
  if(left[1] < top[1]) SWAP(left,top);
  if(right[1] < top[1]) SWAP(right,top);
  if(bot[1] < top[1]) SWAP(bot,top);
  if(bot[1] < left[1]) SWAP(bot,left);
  if(bot[1] < right[1]) SWAP(bot,right);
  // top now definitely contains the highest and bot the lowest.
  if(left[0] > right[0]) SWAP(left,right);
  // left now definitely contains the leftest and right the rightest.
  int y = Q_TO_I(top[1]);
  int rem = Q_TO_I(bot[1] - 1) - Q_TO_I(top[1]);
  if(!rem) return; // not likely given our caller, but this doesn't hurt
  int remtl = Q_TO_I(left[1] - 1) - Q_TO_I(top[1]);
  int remtr = Q_TO_I(right[1] - 1) - Q_TO_I(top[1]);
  Tri_DDA_Vars(l);
  Tri_DDA_Vars(r);
  if(remtl > remtr) {
    Tri_DDA_Init(l, top, left, 1);
    if(remtr) {
      Tri_DDA_Init(r, top, right, 1);
      Quad_StepDualSub(remtr, remtl);
      Tri_DDA_Init(r, right, bot, 1);
      Quad_StepSingleSub(remtl);
      Tri_DDA_Init(l, left, bot, 1);
      Quad_StepSub();
    }
    else {
      Tri_DDA_Init(r, right, bot, 1);
      Quad_StepSub();
    }
  }
  else {
    Tri_DDA_Init(r, top, right, 1);
    if(remtl) {
      Tri_DDA_Init(l, top, left, 1);
      Quad_StepDualSub(remtl, remtr);
      Tri_DDA_Init(l, left, bot, 1);
      Quad_StepSingleSub(remtr);
      Tri_DDA_Init(r, right, bot, 1);
      Quad_StepSub();
    }
    else {
      Tri_DDA_Init(l, left, bot, 1);
      Quad_StepSub();
    }
  }
#undef Quad_StepSub
#undef Quad_StepSingleSub
#undef Quad_StepDualSub
#undef SWAP
}

inline void Drawable::DrawQuadA(const Fixed* top, const Fixed* left, const Fixed* right, const Fixed* bot) {
  const Fixed* sw;
#define SWAP(a,b) do { sw = a; a = b; b = sw; } while(0)
#define Quad_StepDualSub(subsmall, sublarge) \
  while((subsmall)-- > 0) { \
    --(sublarge); --rem;    \
    DrawSpanA(y++, l, r); \
    Tri_DDA_Step(l); \
    Tri_DDA_Step(r); \
  }
#define Quad_StepSingleSub(sub) \
  while((sub)-- > 0) {		\
    --rem; \
    DrawSpanA(y++, l, r); \
    Tri_DDA_Step(l); \
    Tri_DDA_Step(r); \
  }
#define Quad_StepSub() \
  while(rem-- > 0) { \
    DrawSpanA(y++, l, r); \
    Tri_DDA_Step(l); \
    Tri_DDA_Step(r); \
  }
  if(left[1] < top[1]) SWAP(left,top);
  if(right[1] < top[1]) SWAP(right,top);
  if(bot[1] < top[1]) SWAP(bot,top);
  if(bot[1] < left[1]) SWAP(bot,left);
  if(bot[1] < right[1]) SWAP(bot,right);
  // top now definitely contains the highest and bot the lowest.
  if(left[0] > right[0]) SWAP(left,right);
  // left now definitely contains the leftest and right the rightest.
  int y = Q_TO_I(top[1]);
  int rem = Q_TO_I(bot[1] - 1) - Q_TO_I(top[1]);
  if(!rem) return; // not likely given our caller, but this doesn't hurt
  int remtl = Q_TO_I(left[1] - 1) - Q_TO_I(top[1]);
  int remtr = Q_TO_I(right[1] - 1) - Q_TO_I(top[1]);
  Tri_DDA_Vars(l);
  Tri_DDA_Vars(r);
  if(remtl > remtr) {
    Tri_DDA_Init(l, top, left, 1);
    if(remtr) {
      Tri_DDA_Init(r, top, right, 1);
      Quad_StepDualSub(remtr, remtl);
      Tri_DDA_Init(r, right, bot, 1);
      Quad_StepSingleSub(remtl);
      Tri_DDA_Init(l, left, bot, 1);
      Quad_StepSub();
    }
    else {
      Tri_DDA_Init(r, right, bot, 1);
      Quad_StepSub();
    }
  }
  else {
    Tri_DDA_Init(r, top, right, 1);
    if(remtl) {
      Tri_DDA_Init(l, top, left, 1);
      Quad_StepDualSub(remtl, remtr);
      Tri_DDA_Init(l, left, bot, 1);
      Quad_StepSingleSub(remtr);
      Tri_DDA_Init(r, right, bot, 1);
      Quad_StepSub();
    }
    else {
      Tri_DDA_Init(l, left, bot, 1);
      Quad_StepSub();
    }
  }
#undef Quad_StepSub
#undef Quad_StepSingleSub
#undef Quad_StepDualSub
#undef SWAP
}*/

inline void Drawable::DrawQuadLine(Fixed width, Fixed height, const Fixed* top, const Fixed* bot) {
  Fixed radx = width >> 1;
  Fixed rady = height >> 1;
  Fixed off[2] = {-(bot[1] - top[1]), bot[0] - top[0]};
  Fixed mag = QuickFixedSqrt(((long long)off[0]*off[0] + (long long)off[1]*off[1]) >> 6);
  off[0] = off[0] * radx / mag;
  off[1] = off[1] * rady / mag;
  Fixed a[2] = {top[0] - off[0], top[1] - off[1]};
  Fixed b[2] = {top[0] + off[0], top[1] + off[1]};
  Fixed c[2] = {bot[0] - off[0], bot[1] - off[1]};
  Fixed d[2] = {bot[0] + off[0], bot[1] + off[1]};
  /*if(primitive_alpha) DrawQuadA(a, b, c, d);
    else DrawQuad(a, b, c, d);*/
  ClipNDrawTriangle(a, b, c);
  ClipNDrawTriangle(b, c, d);
}

void Drawable::DrawLines(lua_Number width, lua_Number height, const Fixed* coords, const Index* indices, size_t indexcount) throw() {
  size_t n;
  if(width <= 1.0 && height <= 1.0) {
    for(n = 0; n < indexcount - 1; n += 2) {
      DrawBresenline(coords + indices[n] * 2, coords + indices[n+1] * 2);
    }
  }
  else {
    Fixed width_ = F_TO_Q(width);
    Fixed height_ = F_TO_Q(height);
    for(n = 0; n < indexcount - 1; n += 2) {
      DrawQuadLine(width_, height_, coords + indices[n] * 2, coords + indices[n+1] * 2);
    }
  } 
}

void Drawable::DrawLineStrip(lua_Number width, lua_Number height, const Fixed* coords, const Index* indices, size_t indexcount) throw() {
  size_t n;
  if(width <= 1.0 && height <= 1.0) {
    for(n = 0; n < indexcount - 1; ++n) {
      DrawBresenline(coords + indices[n] * 2, coords + indices[n+1] * 2);
    }
  }
  else {
    Fixed width_ = F_TO_Q(width);
    Fixed height_ = F_TO_Q(height);
    for(n = 0; n < indexcount - 1; ++n) {
      DrawQuadLine(width_, height_, coords + indices[n] * 2, coords + indices[n+1] * 2);
    }
  } 
}

void Drawable::DrawLineLoop(lua_Number width, lua_Number height, const Fixed* coords, const Index* indices, size_t indexcount) throw() {
  size_t n;
  if(width <= 1.0 && height <= 1.0) {
    for(n = 0; n < indexcount - 1; ++n) {
      DrawBresenline(coords + indices[n] * 2, coords + indices[n+1] * 2);
    }
    if(indexcount > 2) DrawBresenline(coords + indices[indexcount-1] * 2,
				      coords + indices[0] * 2);
  }
  else {
    Fixed width_ = F_TO_Q(width);
    Fixed height_ = F_TO_Q(height);
    for(n = 0; n < indexcount - 1; ++n) {
      DrawQuadLine(width_, height_, coords + indices[n] * 2, coords + indices[n+1] * 2);
    }
    if(indexcount > 2) DrawQuadLine(width_, height_, coords + indices[indexcount-1] * 2,
				    coords + indices[0] * 2);
  } 
}

void Drawable::DrawTriangles(const Fixed* coords, const Index* indices, size_t indexcount) throw() {
  size_t n;
  for(n = 0; n < indexcount - 2; n += 3) {
    ClipNDrawTriangle(coords + indices[n] * 2, coords + indices[n+1] * 2, coords + indices[n+2] * 2);
  }
}

void Drawable::DrawTriangleStrip(const Fixed* coords, const Index* indices, size_t indexcount) throw() {
  size_t n;
  for(n = 0; n < indexcount - 2; ++n) {
    ClipNDrawTriangle(coords + indices[n] * 2, coords + indices[n+1] * 2, coords + indices[n+2] * 2);
  }
}

void Drawable::DrawTriangleFan(const Fixed* coords, const Index* indices, size_t indexcount) throw() {
  size_t n;
  const Fixed* base = coords + indices[0] * 2;
  for(n = 1; n < indexcount - 1; n += 2) {
    ClipNDrawTriangle(base, coords + indices[n] * 2, coords + indices[n+1] * 2);
  }
}

int Drawable::Lua_DrawLines(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  CoordArray* r = lua_toobject(L, 1, CoordArray);
  IndexArray* i = lua_toobject(L, 2, IndexArray);
  lua_Number width = luaL_checknumber(L, 3);
  lua_Number height = luaL_optnumber(L, 4, width);
  DrawLines(width, height, r->coords, i->indices, i->count);
  return 0;
}

int Drawable::Lua_DrawLineStrip(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  CoordArray* r = lua_toobject(L, 1, CoordArray);
  IndexArray* i = lua_toobject(L, 2, IndexArray);
  lua_Number width = luaL_checknumber(L, 3);
  lua_Number height = luaL_optnumber(L, 4, width);
  DrawLineStrip(width, height, r->coords, i->indices, i->count);
  return 0;
}

int Drawable::Lua_DrawLineLoop(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  CoordArray* r = lua_toobject(L, 1, CoordArray);
  IndexArray* i = lua_toobject(L, 2, IndexArray);
  lua_Number width = luaL_checknumber(L, 3);
  lua_Number height = luaL_optnumber(L, 4, width);
  DrawLineLoop(width, height, r->coords, i->indices, i->count);
  return 0;
}

int Drawable::Lua_DrawTriangles(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  CoordArray* r = lua_toobject(L, 1, CoordArray);
  IndexArray* i = lua_toobject(L, 2, IndexArray);
  size_t count = luaL_optinteger(L, 3, i->count);
  DrawTriangles(r->coords, i->indices, count);
  return 0;
}

int Drawable::Lua_DrawTriangleStrip(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  CoordArray* r = lua_toobject(L, 1, CoordArray);
  IndexArray* i = lua_toobject(L, 2, IndexArray);
  size_t count = luaL_optinteger(L, 3, i->count);
  DrawTriangleStrip(r->coords, i->indices, count);
  return 0;
}

int Drawable::Lua_DrawTriangleFan(lua_State* L) throw() {
  if(has_alpha) return luaL_error(L, "Graphics with alpha channels cannot be modified with this function.");
  CoordArray* r = lua_toobject(L, 1, CoordArray);
  IndexArray* i = lua_toobject(L, 2, IndexArray);
  size_t count = luaL_optinteger(L, 3, i->count);
  DrawTriangleFan(r->coords, i->indices, count);
  return 0;
}

