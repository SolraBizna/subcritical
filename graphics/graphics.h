// -*- c++ -*-
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
#ifndef _SUBCRITICAL_GRAPHICS_H
#define _SUBCRITICAL_GRAPHICS_H

#include "subcritical/core.h"

#define F_TO_Q(f) (Fixed)(f*64)
#define I_TO_Q(f) F_TO_Q(f) // that macro is really type-agnostic
#define Q_TO_I(f) ((f+31)>>6) // rounds down on x.5
#define Q_FLOOR(f) (f>>6)
#define Q_CEIL(f) Q_FLOOR((f)+63)

namespace SubCritical {
  typedef uint32_t Pixel;
  typedef uint8_t Frixel;
  typedef int32_t Fixed;
  typedef uint16_t Index;
  class LOCAL CoordArray : public Object {
  public:
    CoordArray(size_t count);
    virtual ~CoordArray();
    int Lua_GetCount(lua_State* L) const throw();
    PROTOCOL_PROTOTYPE();
    size_t count;
    Fixed* coords;
  };
  class LOCAL IndexArray : public Object {
  public:
    IndexArray(size_t count);
    virtual ~IndexArray();
    int Lua_GetCount(lua_State* L) const throw();
    PROTOCOL_PROTOTYPE();
    size_t count;
    Index* indices;
  };
  // Drivers are likely to be dependent on the exact order of members of this
  // enumeration.
  enum FBLayout {
    FB_xRGB=0, FB_RGBx=1, FB_BGRx=2, FB_xBGR=3,
    FB_ENDIAN_SWAP_MASK=2,
  };
  class EXPORT Frisket : public Object {
  public:
    Frisket(int width, int height);
    virtual ~Frisket();
    int Lua_GetSize(lua_State* L) const throw();
    PROTOCOL_PROTOTYPE();
    int width, height;
    Frixel** rows;
    Frixel* buffer;
  };
  class Drawable;
  class Graphic;
  class EXPORT Drawable : public Object {
  public:
    virtual ~Drawable();
    int Lua_GetSize(lua_State* L) const throw();
    void BlitRect(const Drawable*, int sx, int sy, int sw, int sh, int dx, int dy) throw();
    void Blit(const Drawable*, int dx, int dy) throw();
    void BlitRectT(const Drawable*, int sx, int sy, int sw, int sh, int dx, int dy, lua_Number a) throw();
    void BlitT(const Drawable*, int dx, int dy, lua_Number a) throw();
    int Lua_Blit(lua_State* L) throw();
    void BlitFrisketRect(const Frisket*, int sx, int sy, int sw, int sh, int dx, int dy) throw();
    void BlitFrisket(const Frisket*, int dx, int dy) throw();
    int Lua_BlitFrisket(lua_State* L) throw();
    void SetPrimitiveColor(lua_Number r, lua_Number g, lua_Number b, lua_Number a) throw();
    void DrawPoints(int size, const Fixed* coords, size_t pointcount) throw();
    void DrawLines(lua_Number width, const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawLineStrip(lua_Number width, const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawLineLoop(lua_Number width, const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawTriangles(const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawTriangleStrip(const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawTriangleFan(const Fixed* coords, const Index* indices, size_t indexcount) throw();
    int Lua_SetPrimitiveColor(lua_State* L) throw();
    int Lua_DrawPoints(lua_State* L) throw();
    int Lua_DrawLines(lua_State* L) throw();
    int Lua_DrawLineStrip(lua_State* L) throw();
    int Lua_DrawLineLoop(lua_State* L) throw();
    int Lua_DrawTriangles(lua_State* L) throw();
    int Lua_DrawTriangleStrip(lua_State* L) throw();
    int Lua_DrawTriangleFan(lua_State* L) throw();
    void DrawBox(int l, int t, int r, int b, int sz) throw();
    int Lua_DrawBox(lua_State* L) throw();
    void DrawRect(int l, int t, int r, int b) throw();
    int Lua_DrawRect(lua_State* L) throw();
    void SetClipRect(int l, int t, int r, int b) throw();
    int Lua_SetClipRect(lua_State* L) throw();
    void GetClipRect(int& l, int& t, int& r, int& b) throw();
    int Lua_GetClipRect(lua_State* L) throw();
    int width, height;
    bool has_alpha, simple_alpha, fake_alpha;
    enum FBLayout layout;
    Pixel** rows;
    Pixel* buffer;
    static int PadWidth(int width);
    PROTOCOL_PROTOTYPE();
    int Lua_TakeSnapshot(lua_State* L) throw();
  protected:
    void SetupDrawable(bool invert_y = false) throw();
    void SetupDrawable(void* buffer, int32_t pitch) throw();
    void UpdateShifts();
    Drawable();
  private:
    bool merged_rows;
    int clip_left, clip_top, clip_right, clip_bottom;
    bool primitive_alpha;
    Pixel op_p;
    uint16_t tr_r, tr_g, tr_b, tr_a;
    uint16_t trf_r, trf_g, trf_b, trf_a;
    uint8_t rsh, gsh, bsh, ash;
    LOCAL void DrawSpan(int y, Fixed l, Fixed r);
    LOCAL void DrawSpanA(int y, Fixed l, Fixed r);
    LOCAL void DrawBresenline(const Fixed* a, const Fixed* b);
    LOCAL void DrawBresenlineH(const Fixed* left, const Fixed* right);
    LOCAL void DrawBresenlineV(const Fixed* left, const Fixed* right);
    LOCAL void DrawQuad(const Fixed* top, const Fixed* left, const Fixed* right, const Fixed* bot);
    LOCAL void DrawQuadA(const Fixed* top, const Fixed* left, const Fixed* right, const Fixed* bot);
    LOCAL void DrawQuadLine(Fixed width, const Fixed* top, const Fixed* bot);
    LOCAL void DrawTriangle(const Fixed* a, const Fixed* b, const Fixed* c) throw();
    LOCAL void DrawTriangleL(const Fixed* top, const Fixed* mid, const Fixed* bot) throw();
    LOCAL void DrawTriangleR(const Fixed* top, const Fixed* mid, const Fixed* bot) throw();
  };
  class EXPORT Graphic : public Drawable {
  public:
    Graphic(int width, int height, FBLayout layout);
    Graphic(Drawable& other);
    void CheckAlpha() throw();
    void ChangeLayout(enum FBLayout newlayout) throw();
    int Lua_OptimizeFor(lua_State* L) throw();
    PROTOCOL_PROTOTYPE();
  };
  class EXPORT GraphicsDevice : public Drawable {
  public:
    virtual void Update(int x, int y, int w, int h) throw() = 0;
    virtual void UpdateAll() throw() = 0;
    int Lua_Update(lua_State* L) throw();
    virtual int Lua_GetEvent(lua_State* L) throw() = 0;
    PROTOCOL_PROTOTYPE();
  };
  class EXPORT GraphicLoader : public Object {
  public:
    PROTOCOL_PROTOTYPE();
    virtual Graphic* Load(const char* file) throw() = 0;
    virtual int Lua_Load(lua_State* L) throw();
  };
  class DumpOut;
  class EXPORT GraphicDumper : public Object {
  public:
    PROTOCOL_PROTOTYPE();
    virtual bool Dump(Graphic* graphic, DumpOut& dump, const char*& err) throw() = 0;
    virtual int Lua_Dump(lua_State* L) throw();
  }; 
  class EXPORT DumpOut {
  public:
    bool Write(const void* ptr, unsigned long size);
  private:
    lua_State* L;
  protected:
    DumpOut(lua_State* L);
    friend class GraphicDumper;
  };
  extern EXPORT uint16_t SrgbToLinear[256];
  extern EXPORT uint8_t LinearToSrgb[65536];
}

#endif
