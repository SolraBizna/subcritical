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
#include "subcritical/graphics.h"

#include <string.h>
#include <png.h>
#include <zlib.h>
#include <assert.h>

using namespace SubCritical;

class EXPORT PNGLoader : public GraphicLoader {
 public:
  PNGLoader();
  virtual Graphic* Load(const char* name) throw();
  PROTOCOL_PROTOTYPE();
 private:
  LOCAL static bool CheckSignature(FILE* f) throw();
};

class EXPORT PNGDumper : public GraphicDumper {
 public:
  PNGDumper();
  virtual bool Dump(Graphic* graphic, DumpOut& out, const char*& err) throw();
  PROTOCOL_PROTOTYPE();
  LOCAL int Lua_ListFilters(lua_State* L) throw();
  LOCAL int Lua_SetFilters(lua_State* L) throw();
 private:
  int filters;
};

PROTOCOL_IMP_PLAIN(PNGLoader, GraphicLoader);

static const struct ObjectMethod PNGDMethods[] = {
  METHOD("ListFilters", &PNGDumper::Lua_ListFilters),
  METHOD("SetFilters", &PNGDumper::Lua_SetFilters),
  NOMOREMETHODS()
};
PROTOCOL_IMP(PNGDumper, GraphicDumper, PNGDMethods);

static const char* filternames[] = {"all","none","sub","up","average","paeth",NULL};
static const int filtervalues[] = {PNG_ALL_FILTERS,PNG_FILTER_NONE,PNG_FILTER_SUB,PNG_FILTER_UP,PNG_FILTER_AVG,PNG_FILTER_PAETH};

int PNGDumper::Lua_ListFilters(lua_State* L) throw() {
  lua_newtable(L);
  for(int n = 1; filternames[n]; ++n) {
    lua_pushstring(L, filternames[n]);
    lua_pushvalue(L, -1);
    lua_rawseti(L, -3, n);
    lua_pushboolean(L,1);
    lua_settable(L, -3);
  }
  return 1;
}

int PNGDumper::Lua_SetFilters(lua_State* L) throw() {
  int filters = PNG_NO_FILTERS;
  int top = lua_gettop(L);
  // we want at least one filter specified
  filters |= filtervalues[luaL_checkoption(L, 1, NULL, filternames)];
  for(int n = 2; n <= top; ++n) {
    filters |= filtervalues[luaL_checkoption(L, n, NULL, filternames)];
  }
  assert(filters != PNG_NO_FILTERS);
  this->filters = filters;
  return 0;
}

static void mywrite(png_structp libpng, png_bytep data, png_size_t length) {
  if(!((DumpOut*)png_get_io_ptr(libpng))->Write(data, length)) throw false;
}

static void myflush(png_structp libpng) {
  // no-op
}

static char pngkey[] = "Source";
static char pngvalue[] = "SubCritical game";

bool PNGDumper::Dump(Graphic* graphic, DumpOut& out, const char*& err) throw() {
  png_structp libpng = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!libpng)
    return false;
  png_infop info = png_create_info_struct(libpng);
  if(!info) {
    png_destroy_read_struct(&libpng, NULL, NULL);
    return false;
  }
  png_set_compression_level(libpng, Z_BEST_COMPRESSION);
  png_set_write_fn(libpng, &out, mywrite, myflush);
  try {
    if(setjmp(png_jmpbuf(libpng)))
      throw false;
    /* libpng won't let you specify FILLER_AFTER for png_write_png */
    if(graphic->has_alpha)
      graphic->ChangeLayout(little_endian ? FB_xBGR : FB_RGBx);
    else
      graphic->ChangeLayout(little_endian ? FB_BGRx : FB_xRGB);
    png_set_IHDR(libpng, info, graphic->width, graphic->height, 8, graphic->has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_sRGB_gAMA_and_cHRM(libpng, info, PNG_sRGB_INTENT_PERCEPTUAL);
    png_text comment;
    comment.compression = PNG_TEXT_COMPRESSION_NONE;
    comment.key = pngkey;
    comment.text = pngvalue;
    comment.text_length = strlen(comment.text);
    png_set_text(libpng, info, &comment, 1);
    png_set_filter(libpng, 0, filters);
    png_set_rows(libpng, info, (png_bytepp)graphic->rows);
    png_write_png(libpng, info, PNG_TRANSFORM_IDENTITY|(graphic->has_alpha?0:PNG_TRANSFORM_STRIP_FILLER), NULL);
    png_write_end(libpng, info);
  }
  catch(...) {
    png_destroy_write_struct(&libpng, &info);
    return false;
  }
  png_destroy_write_struct(&libpng, &info);
  return true;
}

bool PNGLoader::CheckSignature(FILE* f) throw() {
  png_byte sig[8];
  if(fread(sig, 1, 8, f) < 8) return false;
  return !png_sig_cmp(sig, 0, 8);
}

Graphic* PNGLoader::Load(const char* name) throw() {
  FILE* f = fopen(name, "rb");
  if(!f) return NULL;
  if(!CheckSignature(f)) {
    fclose(f);
    return NULL;
  }
  png_structp libpng = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!libpng) {
    fclose(f);
    return NULL;
  }
  png_infop info = png_create_info_struct(libpng);
  if(!info) {
    png_destroy_read_struct(&libpng, NULL, NULL);
    fclose(f);
    return NULL;
  }
  Graphic* ret = NULL;
  if(setjmp(png_jmpbuf(libpng))) {
    fclose(f);
    png_destroy_read_struct(&libpng, &info, NULL);
    if(ret) delete ret;
    return NULL;
  }
  png_init_io(libpng, f);
  png_set_sig_bytes(libpng, 8);
  png_read_info(libpng, info);
  png_uint_32 width, height;
  int depth, color_type, ilace_method, compression_method, filter_method;
  png_get_IHDR(libpng, info, &width, &height, &depth, &color_type, &ilace_method, &compression_method, &filter_method);
  // For a 16-bpc image, strip off 8 bits.
  if(depth >= 16) png_set_strip_16(libpng);
  // For a grayscale image, expand G to GGG.
  if(!(color_type & PNG_COLOR_MASK_COLOR)) png_set_gray_to_rgb(libpng);
  // For a palettized image, expand P to RGB.
  if((color_type & PNG_COLOR_TYPE_PALETTE)) png_set_palette_to_rgb(libpng);
  // If there is no alpha channel, fake one.
  png_set_filler(libpng, ~0, PNG_FILLER_AFTER);
  // If there is tRNS, make alpha from it.
  if(png_get_valid(libpng, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(libpng);
  // Note: Perform no gamma correction on incoming PNG data.
  // At this point, we should now have 8-bit RGBA data.
  // Load interlaced PNGs properly.
  (void)png_set_interlace_handling(libpng);
  // Inform libpng of our changes and proceed.
  png_read_update_info(libpng, info);
  ret = new Graphic(width, height, little_endian ? FB_xBGR : FB_RGBx);
  png_read_image(libpng, (png_bytepp)ret->rows);
  png_read_end(libpng, info);
  // Check our alpha properties. (Thanks to the above we can't count on
  // anything!) This is likely a huge CPU hit.
  ret->CheckAlpha();
  fclose(f);
  png_destroy_read_struct(&libpng, &info, NULL);
  return ret;
}

PNGLoader::PNGLoader() {}

SUBCRITICAL_CONSTRUCTOR(PNGLoader)(lua_State* L) {
  (new PNGLoader())->Push(L);
  return 1;
}

PNGDumper::PNGDumper() {}

SUBCRITICAL_CONSTRUCTOR(PNGDumper)(lua_State* L) {
  (new PNGDumper())->Push(L);
  return 1;
}
