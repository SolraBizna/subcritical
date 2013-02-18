// -*- c++ -*-
/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2013 Solra Bizna.

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
#ifndef _SUBCRITICAL_DATA_H
#define _SUBCRITICAL_DATA_H

#include "subcritical/core.h"

#include <string.h>

namespace SubCritical {
  class EXPORT DataBuffer : public Object {
    uint8_t* start, *cur, *end;
    size_t size;
    bool read_only, free_on_gc;
    lua_State* referenced_state;
    bool have_ref_obj, have_callback, in_callback;
    inline void FixupSeek() { if(cur < start) cur = start; else if(cur > end) cur = end; }
    void ClearReferencedObject() LOCAL;
    void ClearCallback() LOCAL;
    void CallCallback() LOCAL;
    inline DataBuffer(DataBuffer&) {} // don't do this.
    inline DataBuffer() {} // or this.
  public:
    // If this buffer is backed by a Lua object, set free_on_gc = false and
    // pass the backing object to SetReferencedObject.
    DataBuffer(void* start, size_t size, bool read_only, bool free_on_gc);
    ~DataBuffer();
    void SetReferencedObject(lua_State* L, int index);
    PROTOCOL_PROTOTYPE();
    inline size_t GetSize() { return size; }
    inline size_t GetLength() { return end - start; }
    inline size_t GetPos() { return cur - start; }
    inline bool HasCallback() { return have_callback; }
    inline size_t GetRemSpace() { return end - cur; }
    inline bool HasRemSpace(size_t rem) { return cur + rem <= end; }
    inline void* GetCurPtr(size_t advance = 0) { void* ret = cur; cur += advance; return ret; }
    inline bool IsReadOnly() { return read_only; }
    inline void UnsafeSeekSet(ptrdiff_t p) { cur = start + p; }
    inline void UnsafeSeekCur(ptrdiff_t p) { cur = cur + p; }
    inline void UnsafeSeekEnd(ptrdiff_t p) { cur = end + p; }
    inline void SeekSet(ptrdiff_t p) { UnsafeSeekSet(p); FixupSeek(); }
    inline void SeekCur(ptrdiff_t p) { UnsafeSeekCur(p); FixupSeek(); }
    inline void SeekEnd(ptrdiff_t p) { UnsafeSeekEnd(p); FixupSeek(); }
    inline void SetSoftEnd() { end = cur; }
    inline void SetSoftEnd(size_t pos) { if(pos > size) pos = size; end = start + pos; if(cur > end) cur = end; }
    inline void ResetSoftEnd() { end = start + size; }
    inline size_t UnsafeCopyOut(void* destination, size_t length) { memcpy(destination, cur, length); return length; }
    inline size_t UnsafeCopyIn(const void* source, size_t length) { memcpy(cur, source, length); return length; }
    inline size_t CopyOut(void* destination, size_t length) {
      if(cur + length > end) length = end - cur;
      return UnsafeCopyOut(destination, length);
    }
    inline size_t CopyIn(const void* source, size_t length) {
      if(read_only) return 0;
      if(cur + length > end) length = end - cur;
      return UnsafeCopyIn(source, length);
    }
    inline size_t UnsafeRead(void* destination, size_t length) {
      size_t amt = UnsafeCopyOut(destination, length);
      cur += amt;
      return amt;
    }
    inline size_t UnsafeWrite(const void* source, size_t length) {
      size_t amt = UnsafeCopyIn(source, length);
      cur += amt;
      return amt;
    }
    /* use these two when possible, because they'll call a set Lua callback if
       the buffer fills up / empties */
    size_t Read(void* destination, size_t length);
    size_t Write(const void* source, size_t length);
    DataBuffer* Clone();
    /* only call this on a DataBuffer with free_on_gc == true (such as a
       clone) */
    bool Resize(size_t new_size);
    int Lua_Resize(lua_State* L);
    int Lua_SetCallback(lua_State* L);
    int Lua_CopyIn(lua_State* L);
    int Lua_CopyOut(lua_State* L);
    int Lua_Read(lua_State* L);
    int Lua_Write(lua_State* L);
    int Lua_GetRemSpace(lua_State* L);
    int Lua_GetSize(lua_State* L);
    int Lua_GetLength(lua_State* L);
    int Lua_GetPos(lua_State* L);
    int Lua_HasCallback(lua_State* L);
    int Lua_IsReadOnly(lua_State* L);
    int Lua_SeekSet(lua_State* L);
    int Lua_SeekCur(lua_State* L);
    int Lua_SeekEnd(lua_State* L);
    int Lua_SetSoftEnd(lua_State* L);
    int Lua_ResetSoftEnd(lua_State* L);
    int Lua_Clone(lua_State* L);
    int Lua_ReadU8(lua_State* L);
    int Lua_ReadU16(lua_State* L);
    int Lua_ReadU32(lua_State* L);
    int Lua_ReadU64(lua_State* L);
    int Lua_ReadS8(lua_State* L);
    int Lua_ReadS16(lua_State* L);
    int Lua_ReadS32(lua_State* L);
    int Lua_ReadS64(lua_State* L);
    int Lua_ReadF32(lua_State* L);
    int Lua_ReadF64(lua_State* L);
    int Lua_WriteU8(lua_State* L);
    int Lua_WriteU16(lua_State* L);
    int Lua_WriteU32(lua_State* L);
    int Lua_WriteU64(lua_State* L);
    int Lua_WriteS8(lua_State* L);
    int Lua_WriteS16(lua_State* L);
    int Lua_WriteS32(lua_State* L);
    int Lua_WriteS64(lua_State* L);
    int Lua_WriteF32(lua_State* L);
    int Lua_WriteF64(lua_State* L);
    int Lua_ReadLittleEndianU16(lua_State* L);
    int Lua_ReadLittleEndianU32(lua_State* L);
    int Lua_ReadLittleEndianU64(lua_State* L);
    int Lua_ReadLittleEndianS16(lua_State* L);
    int Lua_ReadLittleEndianS32(lua_State* L);
    int Lua_ReadLittleEndianS64(lua_State* L);
    int Lua_ReadLittleEndianF32(lua_State* L);
    int Lua_ReadLittleEndianF64(lua_State* L);
    int Lua_WriteLittleEndianU16(lua_State* L);
    int Lua_WriteLittleEndianU32(lua_State* L);
    int Lua_WriteLittleEndianU64(lua_State* L);
    int Lua_WriteLittleEndianS16(lua_State* L);
    int Lua_WriteLittleEndianS32(lua_State* L);
    int Lua_WriteLittleEndianS64(lua_State* L);
    int Lua_WriteLittleEndianF32(lua_State* L);
    int Lua_WriteLittleEndianF64(lua_State* L);
  };
};

#endif
