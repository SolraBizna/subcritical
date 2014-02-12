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
#include "graphics.h"

#include <string.h>

using namespace SubCritical;

class EXPORT SCILoader : public GraphicLoader {
 public:
  SCILoader();
  virtual Graphic* Load(const char* name) throw();
  PROTOCOL_PROTOTYPE();
};

class EXPORT SCIDumper : public GraphicDumper {
 public:
  SCIDumper();
  virtual bool Dump(Graphic* graphic, DumpOut& out, const char*& err) throw();
  PROTOCOL_PROTOTYPE();
};

PROTOCOL_IMP_PLAIN(SCILoader, GraphicLoader);
PROTOCOL_IMP_PLAIN(SCIDumper, GraphicDumper);

SCILoader::SCILoader() {}
SCIDumper::SCIDumper() {}

Graphic* SCILoader::Load(const char* name) throw() {
  FILE* f = fopen(name, "rb");
  if(!f) return NULL;
  {
    char buf[4];
    if(fread(buf, 4, 1, f) < 1) { fclose(f); return NULL; }
    if(memcmp(buf, "SCIM", 4)) { fclose(f); return NULL; }
  }
  uint16_t width, height;
  if(fread(&width, 2, 1, f) < 1) { fclose(f); return NULL; }
  if(fread(&height, 2, 1, f) < 1) { fclose(f); return NULL; }
  width = Swap16_BE(width);
  height = Swap16_BE(height);
  Graphic* ret = new Graphic(width, height, little_endian ? FB_BGRx : FB_xRGB);
  for(Pixel*restrict* p = ret->rows; p < ret->rows + height; ++p) {
    if(fread(*p, width*sizeof(Pixel), 1, f) < 1) { fclose(f); delete ret; return NULL; }
  }
  ret->CheckAlpha();
  return ret;
}

bool SCIDumper::Dump(Graphic* graphic, DumpOut& out, const char*& err) throw() {
  if(!out.Write("SCIM", 4)) { err = "I/O error"; return false; }
  uint16_t sizes[2];
  sizes[0] = Swap16_BE(graphic->width);
  sizes[1] = Swap16_BE(graphic->height);
  if(!out.Write(sizes, 4)) { err = "I/O error"; return false; }
  graphic->ChangeLayout(little_endian ? FB_BGRx : FB_xRGB);
  for(int y = 0; y < graphic->height; ++y) {
    if(!out.Write(graphic->rows[y], graphic->width * 4)) { err = "I/O error"; return false; }
  }
  return true;
}

SUBCRITICAL_CONSTRUCTOR(SCILoader)(lua_State* L) {
  (new SCILoader())->Push(L);
  return 1;
}

SUBCRITICAL_CONSTRUCTOR(SCIDumper)(lua_State* L) {
  (new SCIDumper())->Push(L);
  return 1;
}
