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
#include "subcritical/graphics.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <math.h>
#include <assert.h>

#define SATURATE_255(n) ((n)>255?255:(n))

static FT_Library freetype = NULL;

using namespace SubCritical;

static inline bool IsSpace(uint32_t code) {
  return code == ' ' || code == '\t';
}

static inline int Ceil_26_6(int n) {
  if(n & 63) return (n >> 6) + 1;
  else return n >> 6;
}

static inline int FrellPosition(long pos, size_t len) {
  if(pos < 0) {
    pos += len + 1;
    if(pos < 0) pos = 1;
  }
  else if(pos == 0) pos = 1;
  else if(pos > (long)len) pos = len;
  return pos;
}

static inline uint32_t UTF8_To_UTF32(const char*& string, int& rem) {
  const unsigned char* ustring = (const unsigned char*)string;
  if(ustring[0] < 0x80) {
    --rem;
    ++string;
    return ustring[0];
  }
  else if(ustring[0] < 0xC0) {
    // Invalid, fall through
  }
  else if(ustring[0] < 0xE0 && rem >= 2 && (ustring[1] & 0xC0) == 0x80) {
    rem -= 2;
    string += 2;
    uint32_t ret =  ((ustring[0] & 0x1F) << 6) | (ustring[1] & 0x3F);
    if(ret >= 1 << 7) return ret;
  }
  else if(ustring[0] < 0xF0 && rem >= 3 && (ustring[1] & 0xC0) == 0x80 &&
	  (ustring[2] & 0xC0) == 0x80) {
    rem -= 3;
    string += 3;
    uint32_t ret = ((ustring[0] & 0x0F) << 12) | ((ustring[1] & 0x3F) << 6) | (ustring[2] & 0x3F);
    if(ret >= 1 << 11 && !(ret >= 0xD800 && ret <= 0xDFFF) && ret < 0xFFFE) return ret;
  }
  else if(ustring[0] < 0xF8 && rem >= 4 && (ustring[1] & 0xC0) == 0x80
	  && (ustring[2] & 0xC0) == 0x80 && (ustring[3] & 0xC0) == 0x80) {
    rem -= 4;
    string += 4;
    uint32_t ret = ((ustring[0] & 0x07) << 18) | ((ustring[1] & 0x3F) << 12) | ((ustring[2] & 0x3F) << 6) | (ustring[3] & 0x3F);
    if(ret >= 1 << 16 && ret <= 0x10FFFF) return ret;
  }
  --rem;
  ++string;
  return 0xFFFD; // U+FFFD REPLACEMENT CHARACTER
}

class EXPORT FreetypeFont : public Object {
 public:
  virtual ~FreetypeFont();
  PROTOCOL_PROTOTYPE();
  FT_Face face;
  void SetSize(lua_Number xsize, lua_Number ysize) throw();
  Frisket* RenderText(const char* string, int start, int stop, int width, int subx, int suby, int& outpen) throw();
  int BreakLine(const char* string, size_t len, int width, int start, int& stop) throw();
  int GetTextWidth(const char* string, int start, int stop) throw();
  int GetLineHeight() throw();
  int GetNextChar(const char* string, int rem) throw();
  int GetAscender() throw();
  int Lua_SetSize(lua_State* L) throw();
  int Lua_RenderText(lua_State* L) throw();
  int Lua_BreakLine(lua_State* L) throw();
  int Lua_GetTextWidth(lua_State* L) throw();
  int Lua_GetLineHeight(lua_State* L) throw();
  int Lua_GetNextChar(lua_State* L) throw();
  int Lua_GetAscender(lua_State* L) throw();
  int ascender, renderheight;
};

FreetypeFont::~FreetypeFont() {
  if(face) FT_Done_Face(face);
}

static void CopyToFrisket(FT_Bitmap& bitmap, int sx, Frisket* f, int dx, int dy) {
  assert(bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
  assert(bitmap.num_grays == 256);
  if(bitmap.pitch < 0)
    fprintf(stderr, "WARNING: FreeType bitmap with negative pitch. Untested, may crash.\n");
  int width = bitmap.width - sx;
  if(width <= 0) return;
  uint8_t* sp = bitmap.buffer + sx;
  int height = f->height - dy;
  if(bitmap.rows < height) height = bitmap.rows;
  for(int y = 0; y < height; ++y) {
    Frixel* dp = f->rows[y+dy] + dx;
    int rem = width;
    int x = 0;
    UNROLL_MORE(rem,
		*dp++ = sp[x++]);
    sp += bitmap.pitch;
  }
}

static void SaturateToFrisket(FT_Bitmap& bitmap, int sw, Frisket* f, int dx, int dy) {
  assert(bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
  assert(bitmap.num_grays == 256);
  if(bitmap.pitch < 0)
    fprintf(stderr, "WARNING: FreeType bitmap with negative pitch. Untested, may crash.\n");
  int width = sw;
  if(bitmap.width < sw) sw = width;
  if(width <= 0) return; // should not happen
  uint8_t* sp = bitmap.buffer;
  int height = f->height - dy;
  if(bitmap.rows < height) height = bitmap.rows;
  for(int y = 0; y < height; ++y) {
    Frixel* dp = f->rows[y+dy] + dx;
    int rem = width;
    int x = 0;
    UNROLL_MORE(rem,
		*dp = SATURATE_255(*dp + sp[x]);
		++dp; ++x);
    sp += bitmap.pitch;
  }
}

Frisket* FreetypeFont::RenderText(const char* string, int start, int stop, int width, int subx, int suby, int& outpen) throw() {
  int rem = stop - start + 1;
  if(rem < 0) return 0;
  string += start;
  Frisket* ret = new Frisket(width, renderheight);
  int pen = subx, rdraw = 0;
  FT_UInt left_glyph = 0, right_glyph;
  do {
    uint32_t code = UTF8_To_UTF32(string, rem);
    right_glyph = FT_Get_Char_Index(face, code);
    if(left_glyph && right_glyph) {
      FT_Vector v;
      FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_UNFITTED, &v);
      pen += v.x;
    }
    FT_Vector v;
    v.x = (pen & 63);
    v.y = -suby;
    FT_Set_Transform(face, NULL, &v);
    if(FT_Load_Char(face, code, FT_LOAD_RENDER|FT_LOAD_NO_HINTING|FT_LOAD_LINEAR_DESIGN))
      continue;
    int ldraw = (pen >> 6) + face->glyph->bitmap_left;
    if(ldraw < rdraw) {
      int rp = (rdraw - ldraw);
      SaturateToFrisket(face->glyph->bitmap, rp, ret, ldraw, ascender - face->glyph->bitmap_top);
      CopyToFrisket(face->glyph->bitmap, rp, ret, ldraw + rp, ascender - face->glyph->bitmap_top);
    }
    else
      CopyToFrisket(face->glyph->bitmap, 0, ret, (pen >> 6) + face->glyph->bitmap_left, ascender - face->glyph->bitmap_top);
    int our_rdraw = ldraw + face->glyph->bitmap.width;
    if(our_rdraw > rdraw) rdraw = our_rdraw;
    left_glyph = right_glyph;
    //pen += face->glyph->advance.x;
    pen += (int)(((int64_t)face->glyph->linearHoriAdvance * face->size->metrics.x_scale) >> 16);
  } while(rem > 0);
  // This wastes no more memory than not, and improves performance marginally
  ret->width = rdraw;
  FT_Set_Transform(face, NULL, NULL);
  outpen = pen;
  return ret;
}

int FreetypeFont::Lua_RenderText(lua_State* L) throw() {
  const char* string;
  size_t len;
  string = luaL_checklstring(L, 1, &len);
  if(len == 0) return 0;
  size_t start, stop;
  start = FrellPosition((long)luaL_optnumber(L, 2, 1), len);
  stop = FrellPosition((long)luaL_optnumber(L, 3, -1), len);
  int width;
  if(lua_gettop(L) > 3 && !lua_isnil(L, 4)) width = (int)luaL_checknumber(L, 4);
  else width = GetTextWidth(string, start - 1, stop - 1);
  int subx = (int)(luaL_optnumber(L, 5, 0)*64)&63;
  int suby = (int)(luaL_optnumber(L, 6, 0)*64)&63;
  int outpen;
  RenderText(string, start-1, stop-1, width, subx, suby, outpen)->Push(L);
  lua_pushnumber(L, outpen / 64.0);
  return 2;
}

void FreetypeFont::SetSize(lua_Number xsize, lua_Number ysize) throw() {
  int xs = (int)rint(xsize*64.0);
  int ys = (int)rint(ysize*64.0);
  if(xs < 64) xs = 64;
  else if(xs > 65536) xs = 65536;
  if(ys < 64) ys = 64;
  else if(ys > 65536) ys = 65536;
  FT_Set_Char_Size(face, xs, ys, 72, 72);
  ascender = Ceil_26_6((int)(((int64_t)face->bbox.yMax * face->size->metrics.y_scale) >> 16));
  renderheight = Ceil_26_6((int)(((int64_t)(face->bbox.yMax - face->bbox.yMin) * face->size->metrics.y_scale) >> 16));
}

int FreetypeFont::Lua_SetSize(lua_State* L) throw() {
  lua_Number xsize = luaL_checknumber(L, 1);
  lua_Number ysize = luaL_optnumber(L, 2, 0.0);
  if(xsize == 0.0) {
    if(ysize == 0.0) return luaL_error(L, "At least one point size must be specified");
    else xsize = ysize;
  }
  else if(ysize == 0.0) ysize = xsize;
  SetSize(xsize, ysize);
  return 0;
}

int FreetypeFont::GetTextWidth(const char* string, int start, int stop) throw() {
  int rem = stop - start + 1;
  if(rem < 0) return 0;
  string += start;
  int pen = 0;
  FT_UInt left_glyph = 0, right_glyph;
  do {
    uint32_t code = UTF8_To_UTF32(string, rem);
    right_glyph = FT_Get_Char_Index(face, code);
    if(FT_Load_Char(face, code, FT_LOAD_DEFAULT|FT_LOAD_LINEAR_DESIGN))
      continue;
    if(left_glyph && right_glyph) {
      FT_Vector v;
      FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_UNFITTED, &v);
      pen += v.x;
    }
    left_glyph = right_glyph;
    //pen += face->glyph->advance.x;
    pen += (int)(((int64_t)face->glyph->linearHoriAdvance * face->size->metrics.x_scale) >> 16);
  } while(rem > 0);
  return Ceil_26_6(pen);
}

int FreetypeFont::Lua_GetTextWidth(lua_State* L) throw() {
  const char* string;
  size_t len;
  string = luaL_checklstring(L, 1, &len);
  if(len == 0) return 0;
  size_t start, stop;
  start = FrellPosition((long)luaL_optnumber(L, 2, 1), len);
  stop = FrellPosition((long)luaL_optnumber(L, 3, -1), len);
  lua_pushnumber(L, GetTextWidth(string, start - 1, stop - 1));
  return 1;
}

int FreetypeFont::BreakLine(const char* string, size_t len, int width, int start, int& stop) throw() {
  const char* ostring = string;
  int rem = len - start;
  uint32_t code;
  if(rem < 0 || width < 1) {
    stop = -1;
    return -1;
  }
  string += start;
  int pen = 0, pos = start;
  int last_unbroken_pos = 0, last_broken_pos = -1;
  FT_UInt left_glyph = 0, right_glyph;
  do {
    int opos = pos;
    pos = string - ostring; 
    code = UTF8_To_UTF32(string, rem);
    if(code == '\n') {
      stop = opos + 1;
      if(rem > 0)
	return pos + 2;
      else
	return -1;
    }
    right_glyph = FT_Get_Char_Index(face, code);
    if(FT_Load_Char(face, code, FT_LOAD_DEFAULT))
      continue;
    if(left_glyph && right_glyph) {
      FT_Vector v;
      FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_DEFAULT, &v);
      pen += v.x;
    }
    left_glyph = right_glyph;
    pen += face->glyph->advance.x;
    if(Ceil_26_6(pen) > width) break;
    if(!IsSpace(code))
      last_unbroken_pos = pos;
    else
      last_broken_pos = last_unbroken_pos;
  } while(rem > 0);
  if(last_broken_pos == -1) last_broken_pos = last_unbroken_pos;
  if(rem == 0 && !IsSpace(code) && code != '\n') {
    stop = len;
    return -1;
  }
  stop = last_broken_pos + 1;
  int arem = string - (ostring + last_broken_pos);
  string -= arem;
  rem += arem;
  (void)UTF8_To_UTF32(string, rem);
  code = UTF8_To_UTF32(string, rem);
  if(IsSpace(code)) {
    int lpos;
    do {
      if(rem == 0) return -1;
      lpos = string - ostring;
      code = UTF8_To_UTF32(string, rem);
    } while(IsSpace(code));
    if(code == '\n') {
      if(rem > 0) return lpos + 2;
      else return -1;
    }
    else return lpos + 1;
  }
  else if(rem > 0) return last_broken_pos + 1;
  else return -1;
}

int FreetypeFont::Lua_BreakLine(lua_State* L) throw() {
  const char* string;
  size_t len;
  string = luaL_checklstring(L, 1, &len);
  size_t start;
  int stop;
  start = FrellPosition((long)luaL_optnumber(L, 3, 1), len);
  int nustart = BreakLine(string, len, (size_t)luaL_optnumber(L, 2, 65536.0), start - 1, stop);
  if(stop == -1) lua_pushnil(L);
  else lua_pushnumber(L, stop);
  if(nustart == -1) lua_pushnil(L);
  else lua_pushnumber(L, nustart);
  return 2;
}

int FreetypeFont::GetLineHeight() throw() {
  return Ceil_26_6(face->size->metrics.height);
}

int FreetypeFont::Lua_GetLineHeight(lua_State* L) throw() {
  lua_pushnumber(L, GetLineHeight());
  return 1;
}

int FreetypeFont::GetAscender() throw() {
  return ascender;
}

int FreetypeFont::Lua_GetAscender(lua_State* L) throw() {
  lua_pushnumber(L, GetAscender());
  return 1;
}

int FreetypeFont::GetNextChar(const char* string, int rem) throw() {
  const char* string2 = string;
  (void)UTF8_To_UTF32(string2, rem);
  return string2 - string;
}

int FreetypeFont::Lua_GetNextChar(lua_State* L) throw() {
  const char* string;
  size_t len;
  string = luaL_checklstring(L, 1, &len);
  size_t start, stop;
  start = FrellPosition((long)luaL_optnumber(L, 2, 1), len);
  stop = FrellPosition((long)luaL_optnumber(L, 3, -1), len);
  lua_pushnumber(L, GetNextChar(string + start, start - stop + 1));
  return 1;
}

static const struct ObjectMethod FFMethods[] = {
  METHOD("SetSize", &FreetypeFont::Lua_SetSize),
  METHOD("GetTextWidth", &FreetypeFont::Lua_GetTextWidth),
  METHOD("GetLineHeight", &FreetypeFont::Lua_GetLineHeight),
  METHOD("GetAscender", &FreetypeFont::Lua_GetAscender),
  METHOD("GetNextChar", &FreetypeFont::Lua_GetNextChar),
  METHOD("RenderText", &FreetypeFont::Lua_RenderText),
  METHOD("BreakLine", &FreetypeFont::Lua_BreakLine),
  NOMOREMETHODS(),
};

PROTOCOL_IMP(FreetypeFont, Object, FFMethods);

SUBCRITICAL_CONSTRUCTOR(FreetypeFont)(lua_State* L) {
  if(!freetype) if(FT_Init_FreeType(&freetype)) return luaL_error(L, "FreeType could not be initialized.");
  FreetypeFont* ret = new FreetypeFont();
  int error = FT_New_Face(freetype, GetPath(L, 1),
			  (int)luaL_optnumber(L, 2, 0), &ret->face);
  if(!ret->face) {
    delete ret;
    if(error == FT_Err_Unknown_File_Format)
      return luaL_error(L, "Unknown font format: %s", lua_tostring(L, 1));
    else
      return luaL_error(L, "Could not load font: %s", lua_tostring(L, 1));
  }
  if(!(ret->face->face_flags & FT_FACE_FLAG_SCALABLE)) {
    delete ret;
    return luaL_error(L, "Was not a scalable font: %s", lua_tostring(L, 1));
  }
  ret->SetSize(12.0, 12.0);
  ret->Push(L);
  return 1;
}

LUA_EXPORT int Init_freetype(lua_State* L) {
  fprintf(stderr,"Portions of this SubCritical game's runtime environment are copyright (C) 1996-2001, 2002, 2003, 2004, 2005, 2006, 2007 The FreeType Project (www.freetype.org). All rights reserved.\n(This game ought to display a notice of such on startup if it is not GPL.)\n");
  return 0;
}
