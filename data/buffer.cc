#include "data.h"

#include <stdlib.h>
#include <new>

using namespace SubCritical;

#define REF_OBJ_COOKIE (this)
#define CALLBACK_COOKIE ((uint8_t*)this+1)

DataBuffer::DataBuffer(void* start, size_t size, bool read_only, bool free_on_gc) :
  start((uint8_t*)start), cur((uint8_t*)start), end((uint8_t*)start + size),
  size(size), read_only(read_only), free_on_gc(free_on_gc),
  referenced_state(NULL), have_ref_obj(false), have_callback(false),
  in_callback(false)
{}

void DataBuffer::ClearReferencedObject() {
  lua_State*& L = referenced_state;
  lua_pushlightuserdata(L, REF_OBJ_COOKIE);
  lua_pushnil(L);
  lua_settable(L, LUA_REGISTRYINDEX);
  have_ref_obj = false;
}

void DataBuffer::ClearCallback() {
  lua_State*& L = referenced_state;
  lua_pushlightuserdata(L, CALLBACK_COOKIE);
  lua_pushnil(L);
  lua_settable(L, LUA_REGISTRYINDEX);
  have_callback = false;
}

DataBuffer::~DataBuffer() {
  if(referenced_state) {
    if(have_ref_obj)
      ClearReferencedObject();
    if(have_callback)
      ClearCallback();
  }
  if(start != NULL && free_on_gc) {
    free(start);
    start = NULL;
  }
}

void DataBuffer::SetReferencedObject(lua_State* L, int index) {
  /* this should never happen, but eh */
  if(referenced_state && referenced_state != L)
    luaL_error(L, "BAD BAD error, too many lua_States flying around");
  referenced_state = L;
  lua_pushlightuserdata(L, REF_OBJ_COOKIE);
  if(index < 0) --index;
  lua_pushvalue(L, index);
  lua_settable(L, LUA_REGISTRYINDEX);
}

void DataBuffer::CallCallback() {
  if(in_callback) return;
  in_callback = true;
  lua_State*& L = referenced_state;
  int old_top = lua_gettop(L);
  lua_pushlightuserdata(L, CALLBACK_COOKIE);
  lua_gettable(L, LUA_REGISTRYINDEX);
  int result = lua_pcall(L, 0, 0, 0);
  switch(result) {
  case LUA_ERRRUN:
    fprintf(stderr, "Runtime error during DataBuffer callback call:\n%s\n",
            lua_tostring(L, 1));
    break;
  case LUA_ERRMEM:
    fprintf(stderr, "Memory allocation error during DataBuffer callback call\n");
    break;
  case LUA_ERRERR:
    fprintf(stderr, "Impossible error during DataBuffer callback call\n");
    break;
  case LUA_ERRGCMM:
    fprintf(stderr, "Error during garbage collection during DataBuffer callback call\n");
    break;
  }
  lua_settop(L, old_top);
  in_callback = false;
}

size_t DataBuffer::Read(void* destination, size_t length) {
  size_t amt = CopyOut(destination, length);
  cur += amt;
  if(have_callback && amt != length) {
    CallCallback();
    if(cur != end) return Read((uint8_t*)destination + amt, length - amt);
  }
  return amt;
}

size_t DataBuffer::Write(const void* source, size_t length) {
  size_t amt = CopyIn(source, length);
  cur += amt;
  if(have_callback && (amt != 0 || !read_only) && amt != length) {
    CallCallback();
    if(cur != end) return Write((const uint8_t*)source + amt, length - amt);
  }
  return amt;
}

DataBuffer* DataBuffer::Clone() {
  void* nu_buf = malloc(size);
  if(!nu_buf) throw std::bad_alloc();
  memcpy(nu_buf, start, size);
  return new DataBuffer(nu_buf, size, false, true);
}

bool DataBuffer::Resize(size_t size) {
  if(size == this->size) {
    cur = start;
    end = start + size;
  }
  else if(size == 0) {
    free(start);
    start = NULL;
    cur = NULL;
    end = NULL;
    this->size = size;
  }
  else if(start == NULL) {
    start = (uint8_t*)malloc(size);
    if(start == NULL) return false;
    cur = start;
    end = start + size;
    this->size = size;
  }
  else {
    uint8_t* nu = (uint8_t*)realloc(start, size);
    if(nu) {
      start = nu;
      cur = start;
      end = start + size;
      this->size = size;
    }
    else return false;
  }
  return true;
}

int DataBuffer::Lua_SetCallback(lua_State* L) {
  if(referenced_state && referenced_state != L)
    luaL_error(L, "BAD BAD error, too many lua_States flying around");
  referenced_state = L;
  lua_settop(L, 1);
  lua_pushlightuserdata(L, CALLBACK_COOKIE);
  lua_pushvalue(L, 1);
  lua_settable(L, LUA_REGISTRYINDEX);
  have_callback = lua_type(L,1) != LUA_TNIL;
  return 0;
}

int DataBuffer::Lua_CopyIn(lua_State* L) {
  size_t length;
  const char* source = luaL_checklstring(L, 1, &length);
  size_t amt = CopyIn(source, length);
  if(amt == 0) {
    lua_pushnil(L);
    lua_pushliteral(L, "DataBuffer overflow");
    return 2;
  }
  else {
    lua_pushnumber(L, amt);
    return 1;
  }
}

int DataBuffer::Lua_CopyOut(lua_State* L) {
  lua_Number _length = luaL_checknumber(L, 1);
  size_t length;
  if(_length < 0) length = 0;
  else if(_length > end - cur) length = end - cur;
  else length = _length;
  lua_pushlstring(L, (char*)cur, length);
  return 1;
}

int DataBuffer::Lua_Read(lua_State* L) {
  if(!have_callback) {
    lua_Number _length = luaL_checknumber(L, 1);
    size_t length;
    if(_length < 0) length = 0;
    else if(_length > end - cur) length = end - cur;
    else length = _length;
    lua_pushlstring(L, (char*)cur, length);
    cur += length;
  }
  else {
    lua_Number _length = luaL_checknumber(L, 1);
    size_t length;
    if(_length < 0) length = 0;
    else length = _length;
    /* potentially absurd buffer, yay */
    void* buffer = malloc(length);
    lua_pushlstring(L, (char*)buffer, Read(buffer, length));
    free(buffer);
  }
  return 1;
}

int DataBuffer::Lua_Write(lua_State* L) {
  if(read_only) {
    lua_pushnil(L);
    lua_pushliteral(L, "cannot write to read-only DataBuffer");
    return 2;
  }
  size_t length;
  const char* string = luaL_checklstring(L, 1, &length);
  size_t amt = Write(string, length);
  if(amt == 0) {
    lua_pushnil(L);
    lua_pushliteral(L, "DataBuffer overflow");
    return 2;
  }
  else {
    lua_pushnumber(L, amt);
    return 1;
  }
}

int DataBuffer::Lua_GetSize(lua_State* L) {
  lua_pushnumber(L, GetSize());
  return 1;
}

int DataBuffer::Lua_GetRemSpace(lua_State* L) {
  lua_pushnumber(L, GetRemSpace());
  return 1;
}

int DataBuffer::Lua_GetLength(lua_State* L) {
  lua_pushnumber(L, GetLength());
  return 1;
}

int DataBuffer::Lua_GetPos(lua_State* L) {
  lua_pushnumber(L, GetPos());
  return 1;
}

int DataBuffer::Lua_HasCallback(lua_State* L) {
  lua_pushboolean(L, HasCallback());
  return 1;
}

int DataBuffer::Lua_IsReadOnly(lua_State* L) {
  lua_pushboolean(L, IsReadOnly());
  return 1;
}

int DataBuffer::Lua_SeekSet(lua_State* L) {
  SeekSet(luaL_checkinteger(L, 1));
  return Lua_GetPos(L);
}

int DataBuffer::Lua_SeekCur(lua_State* L) {
  SeekCur(luaL_checkinteger(L, 1));
  return Lua_GetPos(L);
}

int DataBuffer::Lua_SeekEnd(lua_State* L) {
  SeekEnd(luaL_checkinteger(L, 1));
  return Lua_GetPos(L);
}

int DataBuffer::Lua_SetSoftEnd(lua_State* L) {
  if(lua_gettop(L) > 0)
    SetSoftEnd(luaL_checkinteger(L, 1));
  else
    SetSoftEnd();
  return 0;
}

int DataBuffer::Lua_ResetSoftEnd(lua_State* L) {
  ResetSoftEnd();
  return 0;
}

int DataBuffer::Lua_Clone(lua_State* L) {
  Clone()->Push(L);
  return 1;
}

int DataBuffer::Lua_Resize(lua_State* L) {
  lua_pushboolean(L, luaL_checkinteger(L, 1));
  return 1;
}

#define Imp(name,type,swap) \
int DataBuffer::Lua_Read##name(lua_State* L) { \
  union { \
    type q; \
    char buf[sizeof(type)]; \
  } pun; \
  if(Read(pun.buf, sizeof(pun.buf)) < sizeof(pun.buf)) \
    lua_pushnil(L); \
  else \
    lua_pushnumber(L, (type)swap(pun.q)); \
  return 1; \
} \
int DataBuffer::Lua_Write##name(lua_State* L) { \
  if(read_only) { \
    lua_pushboolean(L, false); \
    lua_pushliteral(L, "cannot write to read-only DataBuffer"); \
    return 2; \
  } \
  union { \
    type q; \
    char buf[sizeof(type)]; \
  } pun; \
  pun.q = (type)swap(luaL_checknumber(L, 1)); \
  bool result = Write(pun.buf, sizeof(pun.buf)) == sizeof(pun.buf); \
  lua_pushboolean(L, result); \
  if(!result) lua_pushliteral(L, "DataBuffer overflow"); \
  return 1; \
}

Imp(U8, uint8_t, );
Imp(U16, uint16_t, Swap16_BE);
Imp(U32, uint32_t, Swap32_BE);
Imp(U64, uint64_t, Swap64_BE);
Imp(S8, int8_t, );
Imp(S16, int16_t, Swap16_BE);
Imp(S32, int32_t, Swap32_BE);
Imp(S64, int64_t, Swap64_BE);
Imp(F32, float, SwapF_BE);
Imp(F64, double, SwapD_BE);
Imp(LittleEndianU16, uint16_t, Swap16_LE);
Imp(LittleEndianU32, uint32_t, Swap32_LE);
Imp(LittleEndianU64, uint64_t, Swap64_LE);
Imp(LittleEndianS16, int16_t, Swap16_LE);
Imp(LittleEndianS32, int32_t, Swap32_LE);
Imp(LittleEndianS64, int64_t, Swap64_LE);
Imp(LittleEndianF32, float, SwapF_LE);
Imp(LittleEndianF64, double, SwapD_LE);

static const ObjectMethod DBMethods[] = {
  METHOD("SetCallback", &DataBuffer::Lua_SetCallback),
  METHOD("CopyIn", &DataBuffer::Lua_CopyIn),
  METHOD("CopyOut", &DataBuffer::Lua_CopyOut),
  METHOD("Read", &DataBuffer::Lua_Read),
  METHOD("Write", &DataBuffer::Lua_Write),
  METHOD("GetRemSpace", &DataBuffer::Lua_GetRemSpace),
  METHOD("GetSize", &DataBuffer::Lua_GetSize),
  METHOD("GetLength", &DataBuffer::Lua_GetLength),
  METHOD("GetPos", &DataBuffer::Lua_GetPos),
  METHOD("HasCallback", &DataBuffer::Lua_HasCallback),
  METHOD("IsReadOnly", &DataBuffer::Lua_IsReadOnly),
  METHOD("SeekSet", &DataBuffer::Lua_SeekSet),
  METHOD("SeekCur", &DataBuffer::Lua_SeekCur),
  METHOD("SeekEnd", &DataBuffer::Lua_SeekEnd),
  METHOD("SetSoftEnd", &DataBuffer::Lua_SetSoftEnd),
  METHOD("ResetSoftEnd", &DataBuffer::Lua_ResetSoftEnd),
  METHOD("Clone", &DataBuffer::Lua_Clone),
  METHOD("Resize", &DataBuffer::Lua_Resize),
  METHOD("ReadU8", &DataBuffer::Lua_ReadU8),
  METHOD("ReadU16", &DataBuffer::Lua_ReadU16),
  METHOD("ReadU32", &DataBuffer::Lua_ReadU32),
  METHOD("ReadU64", &DataBuffer::Lua_ReadU64),
  METHOD("ReadS8", &DataBuffer::Lua_ReadS8),
  METHOD("ReadS16", &DataBuffer::Lua_ReadS16),
  METHOD("ReadS32", &DataBuffer::Lua_ReadS32),
  METHOD("ReadS64", &DataBuffer::Lua_ReadS64),
  METHOD("ReadF32", &DataBuffer::Lua_ReadF32),
  METHOD("ReadF64", &DataBuffer::Lua_ReadF64),
  METHOD("WriteU8", &DataBuffer::Lua_WriteU8),
  METHOD("WriteU16", &DataBuffer::Lua_WriteU16),
  METHOD("WriteU32", &DataBuffer::Lua_WriteU32),
  METHOD("WriteU64", &DataBuffer::Lua_WriteU64),
  METHOD("WriteS8", &DataBuffer::Lua_WriteS8),
  METHOD("WriteS16", &DataBuffer::Lua_WriteS16),
  METHOD("WriteS32", &DataBuffer::Lua_WriteS32),
  METHOD("WriteS64", &DataBuffer::Lua_WriteS64),
  METHOD("WriteF32", &DataBuffer::Lua_WriteF32),
  METHOD("WriteF64", &DataBuffer::Lua_WriteF64),
  METHOD("ReadLittleEndianU16", &DataBuffer::Lua_ReadLittleEndianU16),
  METHOD("ReadLittleEndianU32", &DataBuffer::Lua_ReadLittleEndianU32),
  METHOD("ReadLittleEndianU64", &DataBuffer::Lua_ReadLittleEndianU64),
  METHOD("ReadLittleEndianS16", &DataBuffer::Lua_ReadLittleEndianS16),
  METHOD("ReadLittleEndianS32", &DataBuffer::Lua_ReadLittleEndianS32),
  METHOD("ReadLittleEndianS64", &DataBuffer::Lua_ReadLittleEndianS64),
  METHOD("ReadLittleEndianF32", &DataBuffer::Lua_ReadLittleEndianF32),
  METHOD("ReadLittleEndianF64", &DataBuffer::Lua_ReadLittleEndianF64),
  METHOD("WriteLittleEndianU16", &DataBuffer::Lua_WriteLittleEndianU16),
  METHOD("WriteLittleEndianU32", &DataBuffer::Lua_WriteLittleEndianU32),
  METHOD("WriteLittleEndianU64", &DataBuffer::Lua_WriteLittleEndianU64),
  METHOD("WriteLittleEndianS16", &DataBuffer::Lua_WriteLittleEndianS16),
  METHOD("WriteLittleEndianS32", &DataBuffer::Lua_WriteLittleEndianS32),
  METHOD("WriteLittleEndianS64", &DataBuffer::Lua_WriteLittleEndianS64),
  METHOD("WriteLittleEndianF32", &DataBuffer::Lua_WriteLittleEndianF32),
  METHOD("WriteLittleEndianF64", &DataBuffer::Lua_WriteLittleEndianF64),
  NOMOREMETHODS()
};
PROTOCOL_IMP(DataBuffer, Object, DBMethods);

SUBCRITICAL_CONSTRUCTOR(DataBuffer)(lua_State* L) {
  lua_settop(L, 1);
  DataBuffer* ret = NULL;
  switch(lua_type(L, 1)) {
  case LUA_TNUMBER:
    {
      lua_Number size = luaL_checknumber(L, 1);
      if(size < 0) return luaL_error(L, "DataBuffer size must be >= 0");
      void* nu_buf = size ? malloc(size) : NULL;
      if(!nu_buf && size != 0) {
        lua_pushnil(L);
        lua_pushliteral(L, "unable to allocate memory");
        return 2;
      }
      ret = new DataBuffer(nu_buf, size, false, true);
      if(!ret) free(nu_buf);
    }
    break;
  case LUA_TSTRING:
    {
      size_t size;
      const char* buf = luaL_checklstring(L, 1, &size);
      ret = new DataBuffer((void*)buf, size, true, false);
      if(ret) ret->SetReferencedObject(L, 1);
    }
    break;
  default:
    return luaL_error(L, "Expected string or number at arg 1, found %s instead", TypeName(L, 1));
  }
  ret->Push(L);
  if(!ret) {
    lua_pushliteral(L, "unable to allocate memory");
    return 2;
  }
  else return 1;
}
