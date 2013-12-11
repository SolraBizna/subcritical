// -*- c++ -*-
/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2013 Solra Bizna.

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
#ifndef _SUBCRITICAL_BASE_H
#define _SUBCRITICAL_BASE_H

#define _BSD_SOURCE 1
#include <sys/types.h>

#if defined(__MACOSX__) || defined(__APPLE__) || defined(macintosh)
#define Fixed __Fixed__
#include <CoreFoundation/CFByteOrder.h>
#undef Fixed
#define le16toh(x) CFSwapInt16LittleToHost(x)
#define le32toh(x) CFSwapInt32LittleToHost(x)
#define le64toh(x) CFSwapInt64LittleToHost(x)
#define be16toh(x) CFSwapInt16BigToHost(x)
#define be32toh(x) CFSwapInt32BigToHost(x)
#define be64toh(x) CFSwapInt64BigToHost(x)
#define htole16(x) CFSwapInt16HostToLittle(x)
#define htole32(x) CFSwapInt32HostToLittle(x)
#define htole64(x) CFSwapInt64HostToLittle(x)
#define htobe16(x) CFSwapInt16HostToBig(x)
#define htobe32(x) CFSwapInt32HostToBig(x)
#define htobe64(x) CFSwapInt64HostToBig(x)
#endif

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <pthread.h>

/* exactly the same as the check in helper.cc */
#if LUA_VERSION_NUM < 502
#error Lua version too old. Please install Lua 5.2 and make sure scbuild can find your headers.
#elif LUA_VERSION_NUM > 502
#warning Lua version may be too new.
#endif

#if __GNUC__ >= 3
#define restrict __restrict__
#define DEPRECATED __attribute__((deprecated))
#else
#define restrict 
#define DEPRECATED 
#endif

#if NO_DUFF
#define UNROLL(rem, code) do { while(rem >= 4) { rem -= 4; {code;} {code;} {code;} {code;} } switch(rem) { case 3: {code;} case 2: {code;} case 1: {code;} default: break; } } while(0)
#define UNROLL_MORE(rem, code) do { while(rem >= 8) { rem -= 8; {code;} {code;} {code;} {code;} {code;} {code;} {code;} {code;} } switch(rem) { case 7: {code;} case 6: {code;} case 5: {code;} case 4: {code;} case 3: {code;} case 2: {code;} case 1: {code;} default: break; } } while(0)
#else
#define UNROLL(rem, code) do { int _n = ((rem) + 3) >> 2; switch((rem)&3) { case 0: do { {code;} case 3: {code;} case 2: {code;} case 1: {code;} } while(--_n > 0); } } while(0)
#define UNROLL_MORE(rem, code) do { int _n = ((rem) + 7) >> 3; switch((rem)&7) { case 0: do { {code;} case 7: {code;} case 6: {code;} case 5: {code;} case 4: {code;} case 3: {code;} case 2: {code;} case 1: {code;} } while(--_n > 0); } } while(0)
#endif

#include <stdint.h>

#if defined(WIN32) || defined(_WIN32) || defined(HAVE_WINDOWS)
# define EXPORT __declspec(dllexport)
# define IMPORT __declspec(dllimport)
# define LOCAL
#elif HAVE_GCC_VISIBILITY
# define EXPORT __attribute__ ((visibility("default")))
# define IMPORT EXPORT
# define LOCAL __attribute__ ((visibility("hidden")))
#else
# define EXPORT
# define IMPORT
# define LOCAL
#endif
#define LUA_EXPORT extern "C" EXPORT

namespace SubCritical {
  /* Don't use this unless you know you need it. If you don't KNOW you need it,
     YOU DON'T NEED IT. */
  /* TODO: Windows version of this class? */
  class Mutex {
  public:
    inline Mutex() throw() {
      pthread_mutex_init(&mutex, NULL);
      /* Grrr... */
      int oldprio;
      pthread_mutex_setprioceiling(&mutex, 0, &oldprio);
    }
    inline ~Mutex() throw() {
      pthread_mutex_destroy(&mutex);
    }
    inline void Lock() throw() {
      /* We don't check errors, so this is not a kid's toy. */
      pthread_mutex_lock(&mutex);
    }
    inline void Unlock() throw() {
      pthread_mutex_unlock(&mutex);
    }
  private:
    pthread_mutex_t mutex;
  };
  static const bool little_endian = BYTE_ORDER == LITTLE_ENDIAN;
#if BYTE_ORDER == LITTLE_ENDIAN
  static inline uint64_t Swap64(uint64_t u) { return be64toh(u); }
  static inline uint32_t Swap32(uint32_t u) { return be32toh(u); }
  static inline uint16_t Swap16(uint32_t u) { return be16toh(u); }
#else
  static inline uint64_t Swap64(uint64_t u) { return le64toh(u); }
  static inline uint32_t Swap32(uint32_t u) { return le32toh(u); }
  static inline uint16_t Swap16(uint32_t u) { return le16toh(u); }
#endif
  /* swap between big-endian and native (no-ops on big-endian) */
  static inline uint32_t Swap64_BE(uint64_t u) { return be64toh(u); }
  static inline uint32_t Swap32_BE(uint32_t u) { return be32toh(u); }
  static inline uint32_t Swap16_BE(uint16_t u) { return be16toh(u); }
  /* swap between little-endian and native (no-ops on little-endian) */
  static inline uint32_t Swap64_LE(uint64_t u) { return le64toh(u); }
  static inline uint32_t Swap32_LE(uint32_t u) { return le32toh(u); }
  static inline uint32_t Swap16_LE(uint16_t u) { return le16toh(u); }
  /* like above, but for floating-point */
  static inline float SwapF(float f) {
    union { uint32_t u; float f; } wat;
    wat.f = f; wat.u = Swap32(wat.u); return wat.f;
  }
  static inline float SwapF_BE(float f) {
    union { uint32_t u; float f; } wat;
    wat.f = f; wat.u = Swap32_BE(wat.u); return wat.f;
  }
  static inline float SwapF_LE(float f) {
    union { uint32_t u; float f; } wat;
    wat.f = f; wat.u = Swap32_LE(wat.u); return wat.f;
  }
  static inline double SwapD(double f) {
    union { uint64_t u; double f; } wat;
    wat.f = f; wat.u = Swap64(wat.u); return wat.f;
  }
  static inline double SwapD_BE(double f) {
    union { uint64_t u; double f; } wat;
    wat.f = f; wat.u = Swap64_BE(wat.u); return wat.f;
  }
  static inline double SwapD_LE(double f) {
    union { uint64_t u; double f; } wat;
    wat.f = f; wat.u = Swap64_LE(wat.u); return wat.f;
  }
  class Object;
  typedef int(Object::*ScriptMethodPointer)(lua_State* L);
  struct ObjectMethod {
    const char* name;
    ScriptMethodPointer ptr;
  };
#define METHOD(luaname, pointer) {luaname,reinterpret_cast<ScriptMethodPointer>(pointer)}
#define NOMOREMETHODS() {NULL, NULL}
#define MYSUPERIS(superclass) ((const ObjectProtocol*(*)(const Object*) throw())(&superclass::GetProtocol))(this)
#define PROTOCOL_PROTOTYPE() virtual const ObjectProtocol* GetProtocol() const throw()
#define PROTOCOL_IMP(class, super, methods) \
const ObjectProtocol* class::GetProtocol() const throw() { \
  static ObjectProtocol* MyProtocol = NULL; \
  if(!MyProtocol) \
    MyProtocol = new ObjectProtocol(#class, methods, MYSUPERIS(super)); \
  return MyProtocol; \
}
#define PROTOCOL_IMP_PLAIN(class, super) \
const ObjectProtocol* class::GetProtocol() const throw() { \
  static ObjectProtocol* MyProtocol = NULL; \
  if(!MyProtocol) \
    MyProtocol = new ObjectProtocol(#class, MYSUPERIS(super)); \
  return MyProtocol; \
}
  class EXPORT ObjectProtocol {
  public:
    ObjectProtocol(const char* name, const struct ObjectMethod* methods, const ObjectProtocol* super) throw();
    ObjectProtocol(const char* name, const ObjectProtocol* super) throw();
    LOCAL void GetMetatable(lua_State* L, Object* object) const throw();
    LOCAL bool IsA(const char* what) const throw();
    LOCAL int PushIdentity(lua_State* L) const throw();
    const char* name;
  protected:
    LOCAL void SetupIndexTable(lua_State* L, Object* object) const throw();
    const struct ObjectMethod* methods;
    const ObjectProtocol* super;
  };
  class EXPORT Object {
  public:
    virtual ~Object();
    void Push(lua_State* L) throw();
    bool IsA(const char* name) const throw();
    int Lua_IsA(lua_State* L) throw();
    int Lua_Identity(lua_State* L) throw();
    static Object* To(lua_State* L, int n, const char* type = NULL) throw();
#define lua_toobject(L, n, type) ((type*)(Object::To(L, n, #type)))
    // This should return a static ObjectProtocol*. Remember, any Lua functions
    // except virtual functions you overrode from your superclass must be
    // declared by this object!
    virtual const ObjectProtocol* GetProtocol() const throw();
  };
  // awesome userdata that helps to deal cleverly with paths in a portable way
  class EXPORT Path : public Object {
  public:
    Path(const char* path, bool copy = true);
    inline const char* GetPath() const throw() { return path; }
    int Lua_GetPath(lua_State* L) const throw();
    inline const char* GetFilename() const throw() { return filep; }
    int Lua_GetFilename(lua_State* L) const throw();
    virtual ~Path();
    PROTOCOL_PROTOTYPE();
  private:
    char* path, *filep;
    bool shouldfree;
  };
  EXPORT const char* TypeName(lua_State* L, int n);
#define luaL_typerror(L, n, wat) luaL_error(L, "Expected %s at arg %i, found %s instead", wat, n, SubCritical::TypeName(L, n))
  // convenience function
  EXPORT const char* GetPath(lua_State* L, int);
}

#define SUBCRITICAL_CONSTRUCTOR(object) LUA_EXPORT int Construct_##object
#define SUBCRITICAL_UTILITY(name) LUA_EXPORT int Utility_##name

#endif
