#include "subcritical/core.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

using namespace SubCritical;

class LOCAL ProtoPackedArray : public Object {
 public:
  virtual ~ProtoPackedArray() {};
  virtual int Lua_Get(lua_State* L) const throw() = 0;
  virtual int Lua_Set(lua_State* L) throw() = 0;
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
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
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
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
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
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
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
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
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
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
    long z = (long)luaL_checknumber(L, 3);
    if(z < 0 || z >= depth) return luaL_error(L, "array index 3 out of bounds");
    lua_pushnumber(L, (lua_Number)array[x*heightdepth+y*height+z]);
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
    if(x < 0 || x >= width) return luaL_error(L, "array index 1 out of bounds");
    long y = (long)luaL_checknumber(L, 2);
    if(y < 0 || y >= height) return luaL_error(L, "array index 2 out of bounds");
    long z = (long)luaL_checknumber(L, 3);
    if(z < 0 || z >= depth) return luaL_error(L, "array index 3 out of bounds");
    long coord = x*heightdepth+y*height+z;
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
    long coord = x*heightdepth+y*height+z;
    if(val)
      array[coord/32] |= 1<<(coord%32);
    else
      array[coord/32] &= ~(1<<(coord%32));
    return 0;
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

sa(PackedArray1D,     lua_Number, 1, dsum * 8);
sa(PackedArray1D_B,   bool,       1, floor(dsum + 31 / 32) * 4);
sa(PackedArray1D_S8,  int8_t,     1, dsum);
sa(PackedArray1D_U8,  uint8_t,    1, dsum);
sa(PackedArray1D_S16, int16_t,    1, dsum * 2);
sa(PackedArray1D_U16, uint16_t,   1, dsum * 2);
sa(PackedArray1D_S32, int32_t,    1, dsum * 4);
sa(PackedArray1D_U32, uint32_t,   1, dsum * 4);
sa(PackedArray2D,     lua_Number, 2, dsum * 8);
sa(PackedArray2D_B,   bool,       2, floor(dsum + 31 / 32) * 4);
sa(PackedArray2D_S8,  int8_t,     2, dsum);
sa(PackedArray2D_U8,  uint8_t,    2, dsum);
sa(PackedArray2D_S16, int16_t,    2, dsum * 2);
sa(PackedArray2D_U16, uint16_t,   2, dsum * 2);
sa(PackedArray2D_S32, int32_t,    2, dsum * 4);
sa(PackedArray2D_U32, uint32_t,   2, dsum * 4);
sa(PackedArray3D,     lua_Number, 3, dsum * 8);
sa(PackedArray3D_B,   bool,       3, floor(dsum + 31 / 32) * 4);
sa(PackedArray3D_S8,  int8_t,     3, dsum);
sa(PackedArray3D_U8,  uint8_t,    3, dsum);
sa(PackedArray3D_S16, int16_t,    3, dsum * 2);
sa(PackedArray3D_U16, uint16_t,   3, dsum * 2);
sa(PackedArray3D_S32, int32_t,    3, dsum * 4);
sa(PackedArray3D_U32, uint32_t,   3, dsum * 4);
