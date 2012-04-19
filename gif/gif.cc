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
#include "subcritical/graphics.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <gif_lib.h>

#include <setjmp.h>

using namespace SubCritical;

class EXPORT GIFLoader : public GraphicLoader {
 public:
  GIFLoader();
  virtual Graphic* Load(const char* name) throw();
  PROTOCOL_PROTOTYPE();
};

PROTOCOL_IMP_PLAIN(GIFLoader, GraphicLoader);

/* RGB332 color palette with white moved to the front. */
static GifColorType defpal[256] = {
  {  0,  0,  0}, {255,255,255}, {  0,  0, 85}, {  0,  0,170}, {  0,  0,255},
  {  0, 36,  0}, {  0, 36, 85}, {  0, 36,170}, {  0, 36,255}, {  0, 73,  0},
  {  0, 73, 85}, {  0, 73,170}, {  0, 73,255}, {  0,109,  0}, {  0,109, 85},
  {  0,109,170}, {  0,109,255}, {  0,146,  0}, {  0,146, 85}, {  0,146,170},
  {  0,146,255}, {  0,182,  0}, {  0,182, 85}, {  0,182,170}, {  0,182,255},
  {  0,219,  0}, {  0,219, 85}, {  0,219,170}, {  0,219,255}, {  0,255,  0},
  {  0,255, 85}, {  0,255,170}, {  0,255,255}, { 36,  0,  0}, { 36,  0, 85},
  { 36,  0,170}, { 36,  0,255}, { 36, 36,  0}, { 36, 36, 85}, { 36, 36,170},
  { 36, 36,255}, { 36, 73,  0}, { 36, 73, 85}, { 36, 73,170}, { 36, 73,255},
  { 36,109,  0}, { 36,109, 85}, { 36,109,170}, { 36,109,255}, { 36,146,  0},
  { 36,146, 85}, { 36,146,170}, { 36,146,255}, { 36,182,  0}, { 36,182, 85},
  { 36,182,170}, { 36,182,255}, { 36,219,  0}, { 36,219, 85}, { 36,219,170},
  { 36,219,255}, { 36,255,  0}, { 36,255, 85}, { 36,255,170}, { 36,255,255},
  { 73,  0,  0}, { 73,  0, 85}, { 73,  0,170}, { 73,  0,255}, { 73, 36,  0},
  { 73, 36, 85}, { 73, 36,170}, { 73, 36,255}, { 73, 73,  0}, { 73, 73, 85},
  { 73, 73,170}, { 73, 73,255}, { 73,109,  0}, { 73,109, 85}, { 73,109,170},
  { 73,109,255}, { 73,146,  0}, { 73,146, 85}, { 73,146,170}, { 73,146,255},
  { 73,182,  0}, { 73,182, 85}, { 73,182,170}, { 73,182,255}, { 73,219,  0},
  { 73,219, 85}, { 73,219,170}, { 73,219,255}, { 73,255,  0}, { 73,255, 85},
  { 73,255,170}, { 73,255,255}, {109,  0,  0}, {109,  0, 85}, {109,  0,170},
  {109,  0,255}, {109, 36,  0}, {109, 36, 85}, {109, 36,170}, {109, 36,255},
  {109, 73,  0}, {109, 73, 85}, {109, 73,170}, {109, 73,255}, {109,109,  0},
  {109,109, 85}, {109,109,170}, {109,109,255}, {109,146,  0}, {109,146, 85},
  {109,146,170}, {109,146,255}, {109,182,  0}, {109,182, 85}, {109,182,170},
  {109,182,255}, {109,219,  0}, {109,219, 85}, {109,219,170}, {109,219,255},
  {109,255,  0}, {109,255, 85}, {109,255,170}, {109,255,255}, {146,  0,  0},
  {146,  0, 85}, {146,  0,170}, {146,  0,255}, {146, 36,  0}, {146, 36, 85},
  {146, 36,170}, {146, 36,255}, {146, 73,  0}, {146, 73, 85}, {146, 73,170},
  {146, 73,255}, {146,109,  0}, {146,109, 85}, {146,109,170}, {146,109,255},
  {146,146,  0}, {146,146, 85}, {146,146,170}, {146,146,255}, {146,182,  0},
  {146,182, 85}, {146,182,170}, {146,182,255}, {146,219,  0}, {146,219, 85},
  {146,219,170}, {146,219,255}, {146,255,  0}, {146,255, 85}, {146,255,170},
  {146,255,255}, {182,  0,  0}, {182,  0, 85}, {182,  0,170}, {182,  0,255},
  {182, 36,  0}, {182, 36, 85}, {182, 36,170}, {182, 36,255}, {182, 73,  0},
  {182, 73, 85}, {182, 73,170}, {182, 73,255}, {182,109,  0}, {182,109, 85},
  {182,109,170}, {182,109,255}, {182,146,  0}, {182,146, 85}, {182,146,170},
  {182,146,255}, {182,182,  0}, {182,182, 85}, {182,182,170}, {182,182,255},
  {182,219,  0}, {182,219, 85}, {182,219,170}, {182,219,255}, {182,255,  0},
  {182,255, 85}, {182,255,170}, {182,255,255}, {219,  0,  0}, {219,  0, 85},
  {219,  0,170}, {219,  0,255}, {219, 36,  0}, {219, 36, 85}, {219, 36,170},
  {219, 36,255}, {219, 73,  0}, {219, 73, 85}, {219, 73,170}, {219, 73,255},
  {219,109,  0}, {219,109, 85}, {219,109,170}, {219,109,255}, {219,146,  0},
  {219,146, 85}, {219,146,170}, {219,146,255}, {219,182,  0}, {219,182, 85},
  {219,182,170}, {219,182,255}, {219,219,  0}, {219,219, 85}, {219,219,170},
  {219,219,255}, {219,255,  0}, {219,255, 85}, {219,255,170}, {219,255,255},
  {255,  0,  0}, {255,  0, 85}, {255,  0,170}, {255,  0,255}, {255, 36,  0},
  {255, 36, 85}, {255, 36,170}, {255, 36,255}, {255, 73,  0}, {255, 73, 85},
  {255, 73,170}, {255, 73,255}, {255,109,  0}, {255,109, 85}, {255,109,170},
  {255,109,255}, {255,146,  0}, {255,146, 85}, {255,146,170}, {255,146,255},
  {255,182,  0}, {255,182, 85}, {255,182,170}, {255,182,255}, {255,219,  0},
  {255,219, 85}, {255,219,170}, {255,219,255}, {255,255,  0}, {255,255, 85}
};

static int infp(GifFileType* gif, GifByteType* p, int n) {
  return fread(p, 1, n, (FILE*)gif->UserData);
}

Graphic* GIFLoader::Load(const char* name) throw() {
  FILE* f = fopen(name, "rb");
  if(!f) return NULL;
  GifFileType* gif = DGifOpen(f, infp);
  if(!gif) {
   fclose(f);
   return NULL;
  }
  if(DGifSlurp(gif) == GIF_ERROR) {
    DGifCloseFile(gif);
    fclose(f);
    return NULL;
  }
  fclose(f);
  if(gif->ImageCount <= 0) {
    DGifCloseFile(gif);
    return NULL;
  }
  if(gif->SavedImages[0].ImageDesc.Top + gif->SavedImages[0].ImageDesc.Height > gif->SHeight ||
     gif->SavedImages[0].ImageDesc.Left + gif->SavedImages[0].ImageDesc.Width > gif->SWidth) {
    fprintf(stderr, "WARNING: POSSIBLE MALICIOUS GIF FILE DETECTED\nRefusing to read.\n");
    return NULL;
  }
  Graphic* ret = new Graphic(gif->SWidth, gif->SHeight, FB_xRGB);
  GifColorType palette[256];
  memcpy(palette, defpal, sizeof(defpal));
  if(gif->SColorMap) memcpy(palette, gif->SColorMap->Colors, gif->SColorMap->ColorCount * sizeof(*palette));
  if(gif->SavedImages[0].ImageDesc.ColorMap) memcpy(palette, gif->SavedImages[0].ImageDesc.ColorMap->Colors, gif->SavedImages[0].ImageDesc.ColorMap->ColorCount * sizeof(*palette));
  // Clear the "screen" if needed
  if(gif->SavedImages[0].ImageDesc.Top != 0 ||
     gif->SavedImages[0].ImageDesc.Left != 0 ||
     gif->SavedImages[0].ImageDesc.Width != ret->width ||
     gif->SavedImages[0].ImageDesc.Height != ret->height) {
    Pixel bg = 0xFF000000 | (palette[gif->SBackGroundColor].Red << 16) | (palette[gif->SBackGroundColor].Green << 8) | palette[gif->SBackGroundColor].Blue;
    for(int y = 0; y < ret->height; ++y) {
      Pixel* d = ret->rows[y];
      int rem = ret->width;
      UNROLL_MORE(rem,
		  *d++ = bg;);
    }
  }
  uint8_t* s = gif->SavedImages[0].RasterBits;
  if(gif->SavedImages[0].ImageDesc.Interlace) {
    int* rowmap = (int*)malloc(sizeof(int) * gif->SavedImages[0].ImageDesc.Height);
    int i = 0;
    for(int y = 0; y < gif->SavedImages[0].ImageDesc.Height; y += 8)
      rowmap[i++] = y;
    for(int y = 4; y < gif->SavedImages[0].ImageDesc.Height; y += 8)
      rowmap[i++] = y;
    for(int y = 2; y < gif->SavedImages[0].ImageDesc.Height; y += 4)
      rowmap[i++] = y;
    for(int y = 1; y < gif->SavedImages[0].ImageDesc.Height; y += 2)
      rowmap[i++] = y;
    for(int y = 0; y < gif->SavedImages[0].ImageDesc.Height; ++y) {
      Pixel* d = ret->rows[rowmap[y] + gif->SavedImages[0].ImageDesc.Top] + gif->SavedImages[0].ImageDesc.Left;
      int rem = gif->SavedImages[0].ImageDesc.Width;
      UNROLL_MORE(rem,
		  *d++ = 0xFF000000 | (palette[*s].Red << 16) | (palette[*s].Green << 8) | palette[*s].Blue;
		  ++s;);
    }
    free(rowmap);
  }
  else for(int y = 0; y < gif->SavedImages[0].ImageDesc.Height; ++y) {
    Pixel* d = ret->rows[y + gif->SavedImages[0].ImageDesc.Top] + gif->SavedImages[0].ImageDesc.Left;
    int rem = gif->SavedImages[0].ImageDesc.Width;
    UNROLL_MORE(rem,
		*d++ = 0xFF000000 | (palette[*s].Red << 16) | (palette[*s].Green << 8) | palette[*s].Blue;
		++s;);
  }
  DGifCloseFile(gif);
  ret->has_alpha = false;
  return ret;
}

GIFLoader::GIFLoader() {}

SUBCRITICAL_CONSTRUCTOR(GIFLoader)(lua_State* L) {
  (new GIFLoader())->Push(L);
  return 1;
}
