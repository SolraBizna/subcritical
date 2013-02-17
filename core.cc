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

#include "core.h"

#include <string.h>
#include <stdlib.h>

using namespace SubCritical;

#if __GNUC__ >= 3
#define MAY_CONVERT_PMFS 1
#endif

ObjectProtocol::ObjectProtocol(const char* name, const struct ObjectMethod* methods, const ObjectProtocol* super) throw()
: name(name), methods(methods), super(super)
{
}

static const ObjectMethod NoMethods[] = {
  NOMOREMETHODS(),
};

ObjectProtocol::ObjectProtocol(const char* name, const ObjectProtocol* super) throw()
: name(name), methods(NoMethods), super(super)
{
}

static int NIY(lua_State* L) {
  return luaL_error(L, "Not implemented yet");
}

#ifdef MAY_CONVERT_PMFS
typedef int(*FastScriptMethodPointer)(Object*, lua_State* L);

static int CallScriptMethod(lua_State* L) {
  // this alone doubled the execution overhead, and is only necessary if
  // there's trickery going on
  // FastScriptMethodPointer* p = (FastScriptMethodPointer*)luaL_checkudata(L, 1, "FastScriptMethodPointer");
  FastScriptMethodPointer* p = (FastScriptMethodPointer*)lua_topointer(L, 1);
  if(!lua_isuserdata(L, 2)) return luaL_error(L, "This is a method. Use : to call it.");
  Object** ud = (Object**)lua_topointer(L, 2);
  lua_remove(L, 2);
  lua_remove(L, 1);
  return (*p)(*ud, L);
}

static void PushScriptMethod(lua_State* L, ScriptMethodPointer old_ptr, Object* object) {
  if(!old_ptr)
    lua_pushcfunction(L, NIY);
  else {
    FastScriptMethodPointer ptr = (FastScriptMethodPointer)(object->*old_ptr);
    lua_pushlightuserdata(L, (void*)ptr);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if(lua_isnil(L, -1)) {
      lua_pop(L, 1);
      FastScriptMethodPointer* ud = (FastScriptMethodPointer*)lua_newuserdata(L, sizeof(FastScriptMethodPointer));
      *ud = ptr;
      if(luaL_newmetatable(L, "FastScriptMethodPointer")) {
	lua_pushcfunction(L, CallScriptMethod);
	lua_setfield(L, -2, "__call");
      }
      lua_setmetatable(L, -2);
      lua_pushlightuserdata(L, (void*)ptr);
      lua_pushvalue(L, -2);
      lua_settable(L, LUA_REGISTRYINDEX);
    }
  }
}
#else
static int CallScriptMethod(lua_State* L) {
  // ScriptMethodPointer* p = (ScriptMethodPointer*)luaL_checkudata(L, 1, "ScriptMethodPointer");
  ScriptMethodPointer* p = (ScriptMethodPointer*)lua_topointer(L, 1);
  if(!lua_isuserdata(L, 2)) return luaL_error(L, "This is a method. Use : to call it.");
  Object** ud = (Object**)lua_topointer(L, 2);
  lua_remove(L, 2);
  lua_remove(L, 1);
  return (*ud->**p)(L);
}

static void PushScriptMethod(lua_State* L, ScriptMethodPointer ptr, Object* object) {
  if(!ptr)
    lua_pushcfunction(L, NIY);
  else {
    lua_pushlstring(L, (char*)&ptr, sizeof(ptr));
    lua_gettable(L, LUA_REGISTRYINDEX);
    if(lua_isnil(L, -1)) {
      lua_pop(L, 1);
      ScriptMethodPointer* ud = (ScriptMethodPointer*)lua_newuserdata(L, sizeof(ScriptMethodPointer));
      *ud = ptr;
      if(luaL_newmetatable(L, "ScriptMethodPointer")) {
	lua_pushcfunction(L, CallScriptMethod);
	lua_setfield(L, -2, "__call");
      }
      lua_setmetatable(L, -2);
      lua_pushlstring(L, (char*)&ptr, sizeof(ptr));
      lua_pushvalue(L, -2);
      lua_settable(L, LUA_REGISTRYINDEX);
    }
  }
}
#endif

static int FryObject(lua_State* L) {
  Object** ud = (Object**)lua_topointer(L, 1);
  if(*ud) {
    delete(*ud);
    *ud = NULL;
  }
  return 0;
}

void ObjectProtocol::SetupIndexTable(lua_State* L, Object* object) const throw() {
  if(super) super->SetupIndexTable(L, object);
  const struct ObjectMethod* p = methods;
  while(p->name) {
    lua_pushstring(L, p->name);
    PushScriptMethod(L, p->ptr, object);
    lua_settable(L, -3);
    ++p;
  }
}

void ObjectProtocol::GetMetatable(lua_State* L, Object* object) const throw() {
  if(luaL_newmetatable(L, name)) {
    lua_newtable(L);
    SetupIndexTable(L, object);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, FryObject);
    lua_setfield(L, -2, "__gc");
    lua_pushstring(L, name);
    lua_setfield(L, -2, "__type");
  }
}

bool ObjectProtocol::IsA(const char* what) const throw() {
  if(!strcmp(name, what)) return true;
  else if(super) return super->IsA(what);
  else return false;
}

int ObjectProtocol::PushIdentity(lua_State* L) const throw() {
  lua_pushstring(L, name);
  if(super) return super->PushIdentity(L) + 1;
  else return 1;
}

Object* Object::To(lua_State* L, int n, const char* type) throw() {
  if(!lua_isuserdata(L, n)) luaL_error(L, "Expected %s at stack index %d, but found %s instead", type ? type : "a SubCritical object", n, lua_typename(L, lua_type(L, n)));
  Object** ud = (Object**)lua_topointer(L, n);
  if(type && !(*ud)->IsA(type)) luaL_error(L, "Expected %s at stack index %d, but found %s instead", type, n, (*ud)->GetProtocol()->name);
  return *ud;
}

void Object::Push(lua_State* L) throw() {
  if(this == NULL) lua_pushnil(L);
  else {
    Object** ud = (Object**)lua_newuserdata(L, sizeof(Object*));
    *ud = this;
    if(!GetProtocol())
      luaL_error(L, "A class with a NULL ObjectProtocol!");
    GetProtocol()->GetMetatable(L, this);
    lua_setmetatable(L, -2);
  }
}

bool Object::IsA(const char* what) const throw() {
  return GetProtocol()->IsA(what);
}

int Object::Lua_IsA(lua_State* L) throw() {
  const char* what = luaL_checkstring(L, 1);
  lua_pushboolean(L, IsA(what));
  return 1;
}

int Object::Lua_Identity(lua_State* L) throw() {
  return GetProtocol()->PushIdentity(L);
}

Object::~Object() {
}

static const struct ObjectMethod ObjectMethods[] = {
  METHOD("IsA", &Object::Lua_IsA),
  METHOD("Identity", &Object::Lua_Identity),
  NOMOREMETHODS()
};
static ObjectProtocol Object_Protocol("Object", ObjectMethods, NULL);

const ObjectProtocol* Object::GetProtocol() const throw() {
  return &Object_Protocol;
}

const char* SubCritical::TypeName(lua_State* L, int n) {
  if(!lua_isuserdata(L, n)) return lua_typename(L, lua_type(L, n));
  Object** ud = (Object**)lua_topointer(L, n);
  return (*ud)->GetProtocol()->name;
}

LUA_EXPORT int Init_core(lua_State* L) {
  union { uint8_t small; uint32_t large; } endiantest;
  endiantest.large = 1;
  bool really_little_endian = !!endiantest.small;
  if(really_little_endian != little_endian)
    return luaL_error(L, "WRONG RUNTIME ENDIANNESS! This copy of SubCritical was built incorrectly.");
#if defined(i386) || defined(__i386) || defined(__i386__)
  // Change the x87 FPU to use double-precision internally. This doesn't apply
  // when floating point math is being done with SSE, but that's hard to
  // detect, and even in that case, this won't hurt anything or anyone. (For
  // that matter, even when SSE is being used for math, the x87 instructions
  // will still be used for certain operations.)
  // http://www.network-theory.co.uk/docs/gccintro/gccintro_70.html
  unsigned int mode = 0x27F;
  asm("fldcw %0" : : "m" (*&mode));
#endif
  return 0;
}

#if HAVE_WINDOWS
#define SLASH '\\'
#else
#define SLASH '/'
#endif

Path::Path(const char* path, bool copy) : shouldfree(copy) {
  if(copy) {
    size_t l = strlen(path) + 1;
    this->path = (char*)malloc(l);
    memcpy(this->path, path, l);
  }
  else this->path = const_cast<char*>(path);
  filep = strrchr(this->path, SLASH);
  if(filep && *filep) ++filep;
  else filep = this->path;
}

Path::~Path() {
  if(shouldfree) free(path);
}

int Path::Lua_GetPath(lua_State* L) const throw() {
  lua_pushstring(L, path);
  return 1;
}

int Path::Lua_GetFilename(lua_State* L) const throw() {
  lua_pushstring(L, filep);
  return 1;
}

SUBCRITICAL_CONSTRUCTOR(Path)(lua_State* L) {
  (new Path(luaL_checkstring(L, 1)))->Push(L);
  return 1;
}

static const struct ObjectMethod PathMethods[] = {
  METHOD("GetPath", &Path::Lua_GetPath),
  METHOD("GetFilename", &Path::Lua_GetFilename),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Path, Object, PathMethods);

const char* SubCritical::GetPath(lua_State* L, int n) {
  Path* path = lua_toobject(L, n, Path);
  if(!path->IsA("Path")) {
    luaL_typerror(L, n, "Path");
    /* NOTREACHED */
    return NULL;
  }
  return path->GetPath();
}
