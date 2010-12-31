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
#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include <jpeglib.h>
}

#ifdef LOCAL
#undef LOCAL
#endif

#include "subcritical/graphics.h"

#include <string.h>
#include <assert.h>

#include <setjmp.h>

using namespace SubCritical;

class EXPORT JPEGLoader : public GraphicLoader {
 public:
  JPEGLoader();
  virtual Graphic* Load(const char* name) throw();
  PROTOCOL_PROTOTYPE();
};

class EXPORT JPEGDumper : public GraphicDumper {
 public:
  JPEGDumper();
  virtual bool Dump(Graphic* graphic, DumpOut& out, const char*& err) throw();
  int Lua_SetQuality(lua_State* L) throw();
  PROTOCOL_PROTOTYPE();
 private:
  int quality;
};

PROTOCOL_IMP_PLAIN(JPEGLoader, GraphicLoader);

static const struct ObjectMethod JPEGMethods[] = {
  METHOD("SetQuality", &JPEGDumper::Lua_SetQuality),
  NOMOREMETHODS()
};
PROTOCOL_IMP(JPEGDumper, GraphicDumper, JPEGMethods);

struct super_jpeg_decompress_struct {
  struct jpeg_decompress_struct jpeg;
  jmp_buf jmp;
};
struct super_jpeg_compress_struct {
  struct jpeg_compress_struct jpeg;
  jmp_buf jmp;
  struct jpeg_destination_mgr dst;
  DumpOut* out;
  unsigned char buf[4096];
};

int JPEGDumper::Lua_SetQuality(lua_State* L) throw() {
  quality = luaL_checkinteger(L, 1);
  if(quality > 100) quality = 100;
  else if(quality < 0) quality = 0;
  return 0;
}

static void f_jpeg_fataljmp(j_common_ptr C) {
  static char why[JMSG_LENGTH_MAX + 1];
  C->err->format_message(C, why);
  fprintf(stderr, "libjpeg: %s\n", why);
  longjmp(((struct super_jpeg_decompress_struct*)C)->jmp,1);
}

static void f_jpeg_fataljmp_c(j_common_ptr C) {
  static char why[JMSG_LENGTH_MAX + 1];
  C->err->format_message(C, why);
  fprintf(stderr, "libjpeg: %s\n", why);
  longjmp(((struct super_jpeg_compress_struct*)C)->jmp,1);
}

static void f_jpeg_initdest(j_compress_ptr C) {
  struct super_jpeg_compress_struct* c = (struct super_jpeg_compress_struct*)C;
  c->dst.next_output_byte = c->buf;
  c->dst.free_in_buffer = sizeof(c->buf);
}

static boolean f_jpeg_emptyout(j_compress_ptr C) {
  struct super_jpeg_compress_struct* c = (struct super_jpeg_compress_struct*)C;
  if(!c->out->Write(c->buf, sizeof(c->buf))) longjmp(c->jmp,1);
  c->dst.next_output_byte = c->buf;
  c->dst.free_in_buffer = sizeof(c->buf);
  return 1;
}

static void f_jpeg_termdest(j_compress_ptr C) {
  struct super_jpeg_compress_struct* c = (struct super_jpeg_compress_struct*)C;
  if(!c->out->Write(c->buf, c->dst.next_output_byte - c->buf)) longjmp(c->jmp,1);
}

bool JPEGDumper::Dump(Graphic* graphic, DumpOut& out, const char*& err) throw() {
  uint8_t* buf = NULL;
  struct jpeg_error_mgr jerr;
  struct super_jpeg_compress_struct c;
  c.out = &out;
  jpeg_create_compress(&c.jpeg);
  c.jpeg.err = jpeg_std_error(&jerr);
  jerr.error_exit = f_jpeg_fataljmp_c;
  c.jpeg.dest = &c.dst;
  c.dst.init_destination = f_jpeg_initdest;
  c.dst.empty_output_buffer = f_jpeg_emptyout;
  c.dst.term_destination = f_jpeg_termdest;
  buf = (uint8_t*)malloc(graphic->width * 3);
  if(setjmp(c.jmp)) {
    jpeg_destroy_compress(&c.jpeg);
    free(buf);
    err = "Error while compressing";
    return false;
  }
  int rsh, gsh, bsh;
  switch(graphic->layout) {
  default:
    fprintf(stderr, "WARNING: Saving JPEG from unknown format %i!\n", graphic->layout);
  case FB_xRGB:
    rsh = 16;
    gsh = 8;
    bsh = 0;
    break;
  case FB_xBGR:
    rsh = 0;
    gsh = 8;
    bsh = 16;
    break;
  case FB_RGBx:
    rsh = 24;
    gsh = 16;
    bsh = 8;
    break;
  case FB_BGRx:
    rsh = 8;
    gsh = 16;
    bsh = 24;
    break;
  }
  c.jpeg.image_width = graphic->width;
  c.jpeg.image_height = graphic->height;
  c.jpeg.input_components = 3;
  c.jpeg.in_color_space = JCS_RGB;
  jpeg_set_defaults(&c.jpeg);
  jpeg_set_quality(&c.jpeg, quality, 0);
  jpeg_simple_progression(&c.jpeg);
  c.jpeg.optimize_coding = 1;
  jpeg_start_compress(&c.jpeg, 1);
  for(int y = 0; y < graphic->height; ++y) {
    Pixel* s = graphic->rows[y];
    uint8_t* d = buf;
    int rem = graphic->width;
    UNROLL_MORE(rem,
		*d++ = *s >> rsh;
		*d++ = *s >> gsh;
		*d++ = *s >> bsh;
		++s;);
    jpeg_write_scanlines(&c.jpeg, &buf, 1);
  }
  jpeg_finish_compress(&c.jpeg);
  free(buf);
  jpeg_destroy_compress(&c.jpeg);
  return true;
}

Graphic* JPEGLoader::Load(const char* name) throw() {
  FILE* f = fopen(name, "rb");
  if(!f) return NULL;
  struct super_jpeg_decompress_struct c;
  struct jpeg_error_mgr jerr;
  Graphic* ret = NULL;
  uint8_t* buf = NULL;
  c.jpeg.err = jpeg_std_error(&jerr);
  jerr.error_exit = f_jpeg_fataljmp;
  jpeg_create_decompress(&c.jpeg);
  if(setjmp(c.jmp)) {
    jpeg_destroy_decompress(&c.jpeg);
    if(ret) delete ret;
    if(buf) free(buf);
    fclose(f);
    return NULL;
  }
  jpeg_stdio_src(&c.jpeg, f);
  jpeg_read_header(&c.jpeg, TRUE);
  ret = new Graphic(c.jpeg.image_width, c.jpeg.image_height, FB_xRGB);
  jpeg_start_decompress(&c.jpeg);
  buf = (uint8_t*)malloc(sizeof(uint8_t) * ret->width * c.jpeg.out_color_components);
  switch(c.jpeg.out_color_components) {
  default:
    fprintf(stderr, "WARNING: Apparently valid JPEG rejected with a weird (%i) number of color components!\n", c.jpeg.out_color_components);
  case 3:
    for(int y = 0; y < ret->height; ++y) {
      uint8_t* s = buf;
      Pixel* d = ret->rows[y];
      int rem = ret->width;
      jpeg_read_scanlines(&c.jpeg, &buf, 1);
      UNROLL_MORE(rem,
		  *d++ = (s[0] << 16) | (s[1] << 8) | s[2] | 0xFF000000;
		  s += 3;);
    }
    break;
  case 1:
    for(int y = 0; y < ret->height; ++y) {
      uint8_t* s = buf;
      Pixel* d = ret->rows[y];
      int rem = ret->width;
      jpeg_read_scanlines(&c.jpeg, &buf, 1);
      UNROLL_MORE(rem,
		  *d++ = (*s << 16) | (*s << 8) | *s | 0xFF000000;
		  s++;);
    }
    break;
  }
  jpeg_finish_decompress(&c.jpeg);
  jpeg_destroy_decompress(&c.jpeg);
  fclose(f);
  free(buf);
  return ret;
}

JPEGLoader::JPEGLoader() {}

SUBCRITICAL_CONSTRUCTOR(JPEGLoader)(lua_State* L) {
  (new JPEGLoader())->Push(L);
  return 1;
}

JPEGDumper::JPEGDumper() {}

SUBCRITICAL_CONSTRUCTOR(JPEGDumper)(lua_State* L) {
  (new JPEGDumper())->Push(L);
  return 1;
}
