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
#ifndef _SUBCRITICAL_BASE_H
#define _SUBCRITICAL_BASE_H

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if __GNUC__ >= 3
#define restrict __restrict__
#else
#define restrict
#endif

#if NO_DUFF
#define UNROLL(rem, code) do { while(rem >= 4) { rem -= 4; {code;} code; {code;} code; } switch(rem) { case 3: {code;} case 2: {code;} case 1: {code;} default: break; } } while(0)
#define UNROLL_MORE(rem, code) do { while(rem >= 8) { rem -= 8; {code;} code; {code;} code; {code;} code; {code;} code; } switch(rem) { case 7: {code;} case 6: {code;} case 5: {code;} case 4: {code;} case 3: {code;} case 2: {code;} case 1: {code;} default: break; } } while(0)
#else
#define UNROLL(rem, code) do { int _n = ((rem) + 3) >> 2; switch((rem)&3) { case 0: do { {code;} case 3: {code;} case 2: {code;} case 1: {code;} } while(--_n > 0); } } while(0)
#define UNROLL_MORE(rem, code) do { int _n = ((rem) + 7) >> 3; switch((rem)&7) { case 0: do { {code;} case 7: {code;} case 6: {code;} case 5: {code;} case 4: {code;} case 3: {code;} case 2: {code;} case 1: {code;} } while(--_n > 0); } } while(0)
#endif

#include <stdint.h>

#if HAVE_GCC_VISIBILITY
#define EXPORT __attribute__ ((visibility("default")))
#define IMPORT EXPORT
#define LOCAL __attribute__ ((visibility("hidden")))
#elif defined(WIN32) || defined(_WIN32)
#define EXPORT __declspec(dllexport)
#define IMPORT __declspec(dllimport)
#define LOCAL
#else
#define EXPORT
#define IMPORT
#define LOCAL
#endif
#define LUA_EXPORT extern "C" EXPORT

namespace SubCritical {
  extern EXPORT bool little_endian;
  EXPORT uint32_t Swap32(uint32_t);
  EXPORT uint32_t Swap32_BE(uint32_t); // won't swap if we're big-endian
  EXPORT uint32_t Swap32_LE(uint32_t); // won't swap if we're small-endian
  EXPORT uint16_t Swap16(uint16_t);
  EXPORT uint16_t Swap16_BE(uint16_t);
  EXPORT uint16_t Swap16_LE(uint16_t);
  EXPORT float SwapF(float);
  EXPORT float SwapF_BE(float);
  EXPORT float SwapF_LE(float);
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
  // bogus userdata to help with handling of stupid path stuff
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
  // convenience function
  EXPORT const char* GetPath(lua_State* L, int);
}

#define SUBCRITICAL_CONSTRUCTOR(object) LUA_EXPORT int Construct_##object
#define SUBCRITICAL_UTILITY(name) LUA_EXPORT int Utility_##name

#endif
