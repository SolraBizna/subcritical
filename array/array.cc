#include "subcritical/core.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

using namespace SubCritical;

static int EasyDump(lua_State* L, const uint8_t* p, long size) {
  lua_pushlstring(L, (const char*)p, size);
  return 1;
}

static int EasyUndump(lua_State* L, uint8_t* p, long size) {
  size_t l;
  const char* string = luaL_checklstring(L,1,&l);
  if(l != (size_t)size) return luaL_error(L, "dump string wrong size");
  memcpy(p, string, size);
  return 0;
}

template<int ASDF> static int ComplicatedDump(lua_State* L, const uint8_t* p, long size);
template<> int ComplicatedDump<2>(lua_State* L, const uint8_t* p, long size) {
  if(little_endian) {
    uint8_t* buf = (uint8_t*)malloc(size*2);
    uint8_t* q = buf;
    size_t rem = (size_t)size;
    UNROLL(rem, q[0] = p[1]; q[1] = p[0]; q += 2; p += 2;);
    lua_pushlstring(L, (const char*)buf, size*2);
    free(buf);
    return 1;
  }
  else return EasyDump(L, p, size*2);
}
template<> int ComplicatedDump<4>(lua_State* L, const uint8_t* p, long size) {
  if(little_endian) {
    uint8_t* buf = (uint8_t*)malloc(size*4);
    uint8_t* q = buf;
    size_t rem = (size_t)size;
    UNROLL(rem,q[0] = p[3]; q[1] = p[2]; q[2] = p[1]; q[3] = p[0]; q += 4; p += 4;);
    lua_pushlstring(L, (const char*)buf, size*4);
    free(buf);
    return 1;
  }
  else return EasyDump(L, p, size*4);
}
template<> int ComplicatedDump<8>(lua_State* L, const uint8_t* p, long size) {
  if(little_endian) {
    uint8_t* buf = (uint8_t*)malloc(size*8);
    uint8_t* q = buf;
    size_t rem = (size_t)size;
    UNROLL(rem,
           q[0] = p[7]; q[1] = p[6]; q[2] = p[5]; q[3] = p[4]; q[4] = p[3]; q[5] = p[2]; q[6] = p[1]; q[7] = p[0]; q += 8; p += 8;);
    lua_pushlstring(L, (const char*)buf, size*8);
    free(buf);
    return 1;
  }
  else return EasyDump(L, p, size*8);
}

template<int ASDF> static int ComplicatedUndump(lua_State* L, uint8_t* p, long size);
template<> int ComplicatedUndump<2>(lua_State* L, uint8_t* p, long size) {
  if(little_endian) {
    size_t l;
    const char* string = luaL_checklstring(L,1,&l);
    if(l != (size_t)size*2) return luaL_error(L, "dump string wrong size");
    const uint8_t* q = (const uint8_t*)string;
    size_t rem = l/2;
    UNROLL(rem, p[0] = q[1]; p[1] = q[0]; q += 2; p += 2;);
    return 0;
  }
  else return EasyUndump(L, p, size*2);
}
template<> int ComplicatedUndump<4>(lua_State* L, uint8_t* p, long size) {
  if(little_endian) {
    size_t l;
    const char* string = luaL_checklstring(L,1,&l);
    if(l != (size_t)size*4) return luaL_error(L, "dump string wrong size");
    const uint8_t* q = (const uint8_t*)string;
    size_t rem = l/4;
    UNROLL(rem, p[0] = q[3]; p[1] = q[2]; p[2] = q[1]; p[3] = q[0]; q += 4; p += 4;);
    return 0;
  }
  else return EasyUndump(L, p, size*4);
}
template<> int ComplicatedUndump<8>(lua_State* L, uint8_t* p, long size) {
  if(little_endian) {
    size_t l;
    const char* string = luaL_checklstring(L,1,&l);
    if(l != (size_t)size*8) return luaL_error(L, "dump string wrong size");
    const uint8_t* q = (const uint8_t*)string;
    size_t rem = l/8;
    UNROLL(rem, p[0] = q[7]; p[1] = q[6]; p[2] = q[5]; p[3] = q[4]; p[4] = q[3]; p[5] = q[2]; p[6] = q[1]; p[7] = q[0]; q += 8; p += 8;);
    return 0;
  }
  else return EasyUndump(L, p, size*8);
}

class LOCAL ProtoPackedArray : public Object {
 public:
  ProtoPackedArray() : border(0), border_set(false) {}
  virtual ~ProtoPackedArray() {};
  virtual int Lua_Get(lua_State* L) const throw() = 0;
  virtual int Lua_Set(lua_State* L) throw() = 0;
  virtual int Lua_Dump(lua_State* L) const throw() = 0;
  virtual int Lua_Undump(lua_State* L) throw() = 0;
  virtual int Lua_GetSize(lua_State* L) const throw() = 0;
  int Lua_GetBorder(lua_State* L) const throw() {
    return oob(L);
  }
  int Lua_SetBorder(lua_State* L) throw() {
    if(lua_isnil(L, 1)) {
      border_set = false;
      border = 0;
    }
    else {
      border = luaL_checknumber(L, 1);
      border_set = true;
    }
    return 0;
  }
 protected:
  int oob(lua_State* L) const throw() {
    if(border_set) lua_pushnumber(L, border);
    else lua_pushnil(L);
    return 1;
  }
 private:
  lua_Number border;
  bool border_set;
};

template <class T, int order> class PackedArray : public ProtoPackedArray {};

template <class T> class PackedArray<T, 1> : public ProtoPackedArray {
public:
  PackedArray(long dims[1])
    : array((T*)calloc(dims[0],sizeof(T))), width(dims[0]) {
    if(!array) throw 0;
  }
  virtual ~PackedArray() { free(array); }
  virtual int Lua_Get(lua_State* L) const throw() {
    long x = (long)luaL_checknumber(L, 1);
    if(x < 0 || x >= width) return oob(L);
    lua_pushnumber(L, (lua_Number)array[x]);
    return 1;
  }
  virtual int Lua_Set(lua_State* L) throw() {
    T val = (T)luaL_checknumber(L, 1);
    long x = (long)luaL_checknumber(L, 2);
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    array[x] = val;
    return 0;
  }
  virtual int Lua_Dump(lua_State* L) const throw() {
    if(sizeof(T) == 1)
      return EasyDump(L,(const uint8_t*)array,width);
    else
      return ComplicatedDump<sizeof(T)>(L,(const uint8_t*)array,width);
  }
  virtual int Lua_Undump(lua_State* L) throw() {
    if(sizeof(T) == 1)
      return EasyUndump(L,(uint8_t*)array,width);
    else
      return ComplicatedUndump<sizeof(T)>(L,(uint8_t*)array,width);
  }
  virtual int Lua_GetSize(lua_State* L) const throw() {
    lua_pushnumber(L,width);
    return 1;
  }
private:
  T* array;
  long width;
};

template <> class PackedArray<bool, 1> : public ProtoPackedArray {
public:
  PackedArray(long dims[1])
    : array((uint32_t*)calloc((dims[0]+31)/32,sizeof(uint32_t))), width(dims[0]) {
    if(!array) throw 0;
  }
  virtual ~PackedArray() { free(array); }
  virtual int Lua_Get(lua_State* L) const throw() {
    long x = (long)luaL_checknumber(L, 1);
    if(x < 0 || x >= width) return oob(L);
    lua_pushboolean(L, array[x/32] & (1<<(x%32)));
    return 1;
  }
  virtual int Lua_Set(lua_State* L) throw() {
    bool val = (bool)lua_toboolean(L, 1);
    long x = (long)luaL_checknumber(L, 2);
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    if(val)
      array[x/32] |= 1<<(x%32);
    else
      array[x/32] &= ~(1<<(x%32));
    return 0;
  }
  virtual int Lua_Dump(lua_State* L) const throw() {
    return ComplicatedDump<4>(L,(const uint8_t*)array,(width+31)/32);
  }
  virtual int Lua_Undump(lua_State* L) throw() {
    return ComplicatedUndump<4>(L,(uint8_t*)array,(width+31)/32);
  }
  virtual int Lua_GetSize(lua_State* L) const throw() {
    lua_pushnumber(L,width);
    return 1;
  }
private:
  uint32_t* array;
  long width;
};

template <class T> class PackedArray<T, 2> : public ProtoPackedArray {
public:
  PackedArray(long dims[2])
    : array((T*)calloc(dims[0]*dims[1],sizeof(T))), width(dims[0]), height(dims[1]) {
    if(!array) throw 0;
  }
  virtual ~PackedArray() { free(array); }
  virtual int Lua_Get(lua_State* L) const throw() {
    long x = (long)luaL_checknumber(L, 1);
    if(x < 0 || x >= width) return oob(L);
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return oob(L);
    lua_pushnumber(L, (lua_Number)array[x*height+y]);
    return 1;
  }
  virtual int Lua_Set(lua_State* L) throw() {
    T val = (T)luaL_checknumber(L, 1);
    long x = (long)luaL_checknumber(L, 2);
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 3);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
    array[x*height+y] = val;
    return 0;
  }
  virtual int Lua_Dump(lua_State* L) const throw() {
    if(sizeof(T) == 1)
      return EasyDump(L,(const uint8_t*)array,width*height);
    else
      return ComplicatedDump<sizeof(T)>(L,(const uint8_t*)array,width*height);
  }
  virtual int Lua_Undump(lua_State* L) throw() {
    if(sizeof(T) == 1)
      return EasyUndump(L,(uint8_t*)array,width*height);
    else
      return ComplicatedUndump<sizeof(T)>(L,(uint8_t*)array,width*height);
  }
  virtual int Lua_GetSize(lua_State* L) const throw() {
    lua_pushnumber(L,width);
    lua_pushnumber(L,height);
    return 2;
  }
private:
  T* array;
  long width, height;
};

template <> class PackedArray<bool, 2> : public ProtoPackedArray {
public:
  PackedArray(long dims[2])
  : array((uint32_t*)calloc((dims[0]*dims[1]+31)/32,sizeof(uint32_t))), width(dims[0]), height(dims[1]) {
    if(!array) throw 0;
  }
  virtual ~PackedArray() { free(array); }
  virtual int Lua_Get(lua_State* L) const throw() {
    long x = (long)luaL_checknumber(L, 1);
    if(x < 0 || x >= width) return oob(L);
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return oob(L);
    long coord = x*height+y;
    lua_pushboolean(L, array[coord/32] & (1<<(coord%32)));
    return 1;
  }
  virtual int Lua_Set(lua_State* L) throw() {
    bool val = (bool)lua_toboolean(L, 1);
    long x = (long)luaL_checknumber(L, 2);
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 3);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
    long coord = x*height+y;
    if(val)
      array[coord/32] |= 1<<(coord%32);
    else
      array[coord/32] &= ~(1<<(coord%32));
    return 0;
  }
  virtual int Lua_Dump(lua_State* L) const throw() {
    return ComplicatedDump<4>(L,(const uint8_t*)array,((width*height)+31)/32);
  }
  virtual int Lua_Undump(lua_State* L) throw() {
    return ComplicatedUndump<4>(L,(uint8_t*)array,((width*height)+31)/32);
  }
  virtual int Lua_GetSize(lua_State* L) const throw() {
    lua_pushnumber(L,width);
    lua_pushnumber(L,height);
    return 2;
  }
private:
  uint32_t* array;
  long width, height;
};

template <class T> class PackedArray<T, 3> : public ProtoPackedArray {
public:
  PackedArray(long dims[3])
    : array((T*)calloc(dims[0]*dims[1]*dims[2],sizeof(T))), width(dims[0]), height(dims[1]), depth(dims[2]), heightdepth(dims[1]*dims[2]) {
    if(!array) throw 0;
  }
  virtual ~PackedArray() { free(array); }
  virtual int Lua_Get(lua_State* L) const throw() {
    long x = (long)luaL_checknumber(L, 1);
    if(x < 0 || x >= width) return oob(L);
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return oob(L);
    long z = (long)luaL_checknumber(L, 3);
    if(z < 0 || z >= depth) return oob(L);
    lua_pushnumber(L, (lua_Number)array[x*heightdepth+y*depth+z]);
    return 1;
  }
  virtual int Lua_Set(lua_State* L) throw() {
    T val = (T)luaL_checknumber(L, 1);
    long x = (long)luaL_checknumber(L, 2);
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 3);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
    long z = (long)luaL_checknumber(L, 4);
    if(z < 0 || z >= depth) return luaL_error(L, "array index 3 out of bounds");
    array[x*heightdepth+y*depth+z] = val;
    return 0;
  }
  virtual int Lua_Dump(lua_State* L) const throw() {
    if(sizeof(T) == 1)
      return EasyDump(L,(const uint8_t*)array,width*heightdepth);
    else
      return ComplicatedDump<sizeof(T)>(L,(const uint8_t*)array,width*heightdepth);
  }
  virtual int Lua_Undump(lua_State* L) throw() {
    if(sizeof(T) == 1)
      return EasyUndump(L,(uint8_t*)array,width*heightdepth);
    else
      return ComplicatedUndump<sizeof(T)>(L,(uint8_t*)array,width*heightdepth);
  }
  virtual int Lua_GetSize(lua_State* L) const throw() {
    lua_pushnumber(L,width);
    lua_pushnumber(L,height);
    lua_pushnumber(L,depth);
    return 3;
  }
private:
  T* array;
  long width, height, depth, heightdepth;
};

template <> class PackedArray<bool, 3> : public ProtoPackedArray {
public:
  PackedArray(long dims[3])
    : array((uint32_t*)calloc((dims[0]*dims[1]*dims[2]+31)/32,sizeof(uint32_t))), width(dims[0]), height(dims[1]), depth(dims[2]), heightdepth(dims[0]*dims[1]) {
    if(!array) throw 0;
  }
  virtual ~PackedArray() { free(array); }
  virtual int Lua_Get(lua_State* L) const throw() {
    long x = (long)luaL_checknumber(L, 1);
    if(x < 0 || x >= width) return oob(L);
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return oob(L);
    long z = (long)luaL_checknumber(L, 3);
    if(z < 0 || z >= depth) return oob(L);
    long coord = x*heightdepth+y*depth+z;
    lua_pushboolean(L, array[coord/32] & (1<<(coord%32)));
    return 1;
  }
  virtual int Lua_Set(lua_State* L) throw() {
    bool val = (bool)lua_toboolean(L, 1);
    long x = (long)luaL_checknumber(L, 2);
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 3);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
    long z = (long)luaL_checknumber(L, 4);
    if(z < 0 || z >= depth) return luaL_error(L, "array index 3 out of bounds");
    long coord = x*heightdepth+y*depth+z;
    if(val)
      array[coord/32] |= 1<<(coord%32);
    else
      array[coord/32] &= ~(1<<(coord%32));
    return 0;
  }
  virtual int Lua_Dump(lua_State* L) const throw() {
    return ComplicatedDump<4>(L,(const uint8_t*)array,((width*heightdepth)+31)/32);
  }
  virtual int Lua_Undump(lua_State* L) throw() {
    return ComplicatedUndump<4>(L,(uint8_t*)array,((width*heightdepth)+31)/32);
  }
  virtual int Lua_GetSize(lua_State* L) const throw() {
    lua_pushnumber(L,width);
    lua_pushnumber(L,height);
    lua_pushnumber(L,depth);
    return 3;
  }
private:
  uint32_t* array;
  long width, height, depth, heightdepth;
};

#define sa(name, type, order, sizeop) \
class LOCAL name : public PackedArray<type, order> { \
public: \
  PROTOCOL_PROTOTYPE(); \
  name(long dims[1]) : PackedArray<type, order>(dims) {} \
}; \
static const struct ObjectMethod name##_Methods[] = { \
  METHOD("Get", &name::Lua_Get), \
  METHOD("Set", &name::Lua_Set), \
  METHOD("GetBorder", &name::Lua_GetBorder), \
  METHOD("SetBorder", &name::Lua_SetBorder), \
  METHOD("Dump", &name::Lua_Dump), \
  METHOD("Undump", &name::Lua_Undump), \
  METHOD("GetSize", &name::Lua_GetSize), \
  NOMOREMETHODS(), \
}; \
PROTOCOL_IMP(name, Object, name##_Methods); \
SUBCRITICAL_CONSTRUCTOR(name)(lua_State* L) { \
  long dims[order]; \
  double dsum = 1.0; \
  int n; \
  for(n = 0; n < order; ++n) { \
    dims[n] = (long)luaL_checknumber(L, n+1); \
    if(dims[n] < 1) return luaL_error(L, "dimension %d too small", n+1); \
    dsum *= dims[n]; \
  } \
  dsum = sizeop; \
  if((double)(long)dsum != dsum) return luaL_error(L, "dimensions too large");\
  try { \
    (new name(dims))->Push(L); \
    return 1; \
  } \
  catch(...) { \
    return luaL_error(L, "memory allocation failure"); \
  } \
}

sa(PackedArray1D,     lua_Number, 1, dsum * sizeof(lua_Number));
sa(PackedArray1D_B,   bool,       1, floor(dsum + 31 / 32) * 4);
sa(PackedArray1D_S8,  int8_t,     1, dsum);
sa(PackedArray1D_U8,  uint8_t,    1, dsum);
sa(PackedArray1D_S16, int16_t,    1, dsum * 2);
sa(PackedArray1D_U16, uint16_t,   1, dsum * 2);
sa(PackedArray1D_S32, int32_t,    1, dsum * 4);
sa(PackedArray1D_U32, uint32_t,   1, dsum * 4);
sa(PackedArray1D_F32, float,      1, dsum * sizeof(float));
sa(PackedArray1D_F64, double,     1, dsum * sizeof(double));
sa(PackedArray2D,     lua_Number, 2, dsum * sizeof(lua_Number));
sa(PackedArray2D_B,   bool,       2, floor(dsum + 31 / 32) * 4);
sa(PackedArray2D_S8,  int8_t,     2, dsum);
sa(PackedArray2D_U8,  uint8_t,    2, dsum);
sa(PackedArray2D_S16, int16_t,    2, dsum * 2);
sa(PackedArray2D_U16, uint16_t,   2, dsum * 2);
sa(PackedArray2D_S32, int32_t,    2, dsum * 4);
sa(PackedArray2D_U32, uint32_t,   2, dsum * 4);
sa(PackedArray2D_F32, float,      2, dsum * sizeof(float));
sa(PackedArray2D_F64, double,     2, dsum * sizeof(double));
sa(PackedArray3D,     lua_Number, 3, dsum * sizeof(lua_Number));
sa(PackedArray3D_B,   bool,       3, floor(dsum + 31 / 32) * 4);
sa(PackedArray3D_S8,  int8_t,     3, dsum);
sa(PackedArray3D_U8,  uint8_t,    3, dsum);
sa(PackedArray3D_S16, int16_t,    3, dsum * 2);
sa(PackedArray3D_U16, uint16_t,   3, dsum * 2);
sa(PackedArray3D_S32, int32_t,    3, dsum * 4);
sa(PackedArray3D_U32, uint32_t,   3, dsum * 4);
sa(PackedArray3D_F32, float,      3, dsum * sizeof(float));
sa(PackedArray3D_F64, double,     3, dsum * sizeof(double));
