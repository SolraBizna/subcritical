// -*- c++ -*-
/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2009 Solra Bizna.

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

#define F_TO_Q(f) (Fixed)((f)*64)
#define I_TO_Q(f) F_TO_Q(f) // that macro is really type-agnostic
#define Q_TO_I(f) (((f)+31)>>6) // rounds down on x.5
#define Q_FLOOR(f) ((f)>>6)
#define Q_CEIL(f) Q_FLOOR((f)+63)

namespace SubCritical {
  typedef uint32_t Pixel;
  typedef uint8_t Frixel;
  typedef int32_t Fixed;
  typedef uint16_t Index;
  class EXPORT CoordArray : public Object {
  public:
    CoordArray(size_t count);
    virtual ~CoordArray();
    int Lua_GetCount(lua_State* L) const throw();
    PROTOCOL_PROTOTYPE();
    size_t count;
    Fixed* coords;
  };
  class EXPORT IndexArray : public Object {
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
    void CopyFrisketRect(const Frisket*restrict, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw();
    void CopyFrisket(const Frisket*restrict, int dx, int dy) restrict throw();
    int Lua_CopyFrisket(lua_State* L) restrict throw();
    void ModulateFrisketRect(const Frisket*restrict, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw();
    void ModulateFrisket(const Frisket*restrict, int dx, int dy) restrict throw();
    int Lua_ModulateFrisket(lua_State* L) restrict throw();
    void AddFrisketRect(const Frisket*restrict, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw();
    void AddFrisket(const Frisket*restrict, int dx, int dy) restrict throw();
    int Lua_AddFrisket(lua_State* L) restrict throw();
    void SubtractFrisketRect(const Frisket*restrict, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw();
    void SubtractFrisket(const Frisket*restrict, int dx, int dy) restrict throw();
    int Lua_SubtractFrisket(lua_State* L) restrict throw();
    void MinFrisketRect(const Frisket*restrict, int sx, int sy, int sw, int sh, int dx, int dy) throw();
    void MinFrisket(const Frisket*restrict, int dx, int dy) restrict throw();
    int Lua_MinFrisket(lua_State* L) restrict throw();
    void MaxFrisketRect(const Frisket*restrict, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw();
    void MaxFrisket(const Frisket*restrict, int dx, int dy) restrict throw();
    int Lua_MaxFrisket(lua_State* L) restrict throw();
    int Lua_GetSize(lua_State* L) const throw();
    PROTOCOL_PROTOTYPE();
    int width, height;
    Frixel*restrict* rows;
    Frixel* buffer;
  };
  class Drawable;
  class Graphic;
  class EXPORT Drawable : public Object {
  public:
    virtual ~Drawable();
    int Lua_GetSize(lua_State* L) const throw();
    void CopyRect(const Drawable*restrict, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw();
    void Copy(const Drawable*restrict, int dx, int dy) restrict throw();
    void BlitRect(const Drawable*restrict, int sx, int sy, int sw, int sh, int dx, int dy) restrict throw();
    void Blit(const Drawable*restrict, int dx, int dy) restrict throw();
    void BlitRectT(const Drawable*restrict, int sx, int sy, int sw, int sh, int dx, int dy, lua_Number a) restrict throw();
    void BlitT(const Drawable*restrict, int dx, int dy, lua_Number a) restrict throw();
    int Lua_Copy(lua_State* L) restrict throw();
    int Lua_Blit(lua_State* L) restrict throw();
    void BlitFrisketRect(const Frisket*, int sx, int sy, int sw, int sh, int dx, int dy) throw();
    void BlitFrisket(const Frisket*, int dx, int dy) throw();
    int Lua_BlitFrisket(lua_State* L) throw();
    void SetPrimitiveColor(lua_Number r, lua_Number g, lua_Number b, lua_Number a) throw();
    void SetPrimitiveColorPremul(lua_Number r, lua_Number g, lua_Number b, lua_Number a) throw();
    void DrawPoints(int size, const Fixed* coords, size_t pointcount) throw();
    void DrawLines(lua_Number width, lua_Number height, const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawLineStrip(lua_Number width, lua_Number height, const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawLineLoop(lua_Number width, lua_Number height, const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawTriangles(const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawTriangleStrip(const Fixed* coords, const Index* indices, size_t indexcount) throw();
    void DrawTriangleFan(const Fixed* coords, const Index* indices, size_t indexcount) throw();
    int Lua_SetPrimitiveColor(lua_State* L) throw();
    int Lua_SetPrimitiveColorPremul(lua_State* L) throw();
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
    Pixel*restrict* rows;
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
    uint16_t tr_r, tr_g, tr_b; uint32_t tr_a;
    uint16_t trf_r, trf_g, trf_b, trf_a;
    uint8_t rsh, gsh, bsh, ash;
    //LOCAL void DrawSpan(int y, Fixed l, Fixed r);
    //LOCAL void DrawSpanA(int y, Fixed l, Fixed r);
    LOCAL void NoclipDrawSpan(int y, Fixed l, Fixed r);
    LOCAL void NoclipDrawSpanA(int y, Fixed l, Fixed r);
    LOCAL void DrawBresenline(const Fixed*restrict a, const Fixed*restrict b);
    LOCAL void DrawBresenlineH(const Fixed*restrict left, const Fixed*restrict right);
    LOCAL void DrawBresenlineV(const Fixed*restrict left, const Fixed*restrict right);
    //LOCAL void DrawQuad(const Fixed* top, const Fixed* left, const Fixed* right, const Fixed* bot);
    //LOCAL void DrawQuadA(const Fixed* top, const Fixed* left, const Fixed* right, const Fixed* bot);
    LOCAL void DrawQuadLine(Fixed width, Fixed height, const Fixed*restrict top, const Fixed*restrict bot);
    LOCAL void ClipNDrawTriangle(const Fixed*restrict a, const Fixed*restrict b, const Fixed*restrict c) throw();
    LOCAL void DrawTriangle(const Fixed*restrict a, const Fixed*restrict b, const Fixed*restrict c) throw();
    LOCAL void DrawTriangleL(const Fixed*restrict top, const Fixed*restrict mid, const Fixed*restrict bot) throw();
    LOCAL void DrawTriangleR(const Fixed*restrict top, const Fixed*restrict mid, const Fixed*restrict bot) throw();
  };
  class EXPORT Graphic : public Drawable {
  public:
    Graphic(int width, int height, FBLayout layout);
    Graphic(Drawable& other);
    void CheckAlpha() throw();
    void ChangeLayout(enum FBLayout newlayout) throw();
    int Lua_OptimizeFor(lua_State* L) restrict throw();
    PROTOCOL_PROTOTYPE();
  };
  class EXPORT GraphicsDevice : public Drawable {
  public:
    virtual void Update(int x, int y, int w, int h) throw() = 0;
    virtual void UpdateAll() throw() = 0;
    int Lua_Update(lua_State* L) throw();
    virtual int Lua_GetEvent(lua_State* L) throw() = 0;
    virtual int Lua_GetMousePos(lua_State* L) throw() = 0;
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
