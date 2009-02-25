/*
  This source file is part of the SubCritical kernel.
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

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <string.h>
#include <assert.h>

// This needs to be synced with core.h should either code change.
#if HAVE_GCC_VISIBILITY
#define EXPORT __attribute__ ((visibility("default")))
#define LOCAL __attribute__ ((visibility("hidden")))
#else
#define EXPORT
#define LOCAL
#endif
#define LUA_EXPORT extern "C" EXPORT

#ifdef HAVE_WINDOWS
# define OS "windows"
# error NIY
#else
# include <dlfcn.h>
# include <dirent.h>
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
# include <string.h>
# include <sys/stat.h>
# ifdef __MACOSX__
#  define OS "macosx"
# else
#  define OS "unix"
# endif
#endif

#include <stdlib.h>
#include <stdio.h>
static void* amalloc(size_t length) {
  void* ret = malloc(length);
  if(!ret) {
    fprintf(stderr, "Memory allocation failure!\n");
    abort();
  }
  return ret;
}

static int f_listfiles(lua_State* L) {
  struct dirent* ent;
  const char* ext = luaL_checkstring(L, 2);
  DIR* dir = opendir(luaL_checkstring(L, 1));
  size_t extlen = strlen(ext);
  int i;
  if(!lua_istable(L, -1))
    lua_newtable(L);
  if(!dir) return 1;
  i = lua_objlen(L, -1) + 1;
  while((ent = readdir(dir))) {
    if(strlen(ent->d_name) < extlen) continue; // lol
    if(strcasecmp(ent->d_name + strlen(ent->d_name) - extlen, ext)) continue;
    lua_pushvalue(L, 1);
    lua_pushstring(L, ent->d_name);
    lua_concat(L, 2);
    lua_rawseti(L, -2, i++);
  }
  closedir(dir);
  return 1;
}

static int f_dlopen(lua_State* L) {
  void* lib = dlopen(luaL_checkstring(L, 1), RTLD_NOW|RTLD_GLOBAL);
  if(!lib) {
    lua_pushnil(L);
    lua_pushstring(L, dlerror());
    return 2;
  }
  else {
    lua_pushlightuserdata(L, lib);
    return 1;
  }
}

static int f_dlsym(lua_State* L) {
  void* sym = dlsym((void*)lua_topointer(L, 1), luaL_checkstring(L, 2));
  if(!sym) {
    lua_pushnil(L);
    lua_pushstring(L, dlerror());
    return 2;
  }
  else {
    lua_pushcfunction(L, (lua_CFunction)sym);
    return 1;
  }
}

static int f_chdir(lua_State* L) {
  if(chdir(luaL_checkstring(L, 1)))
    return luaL_error(L, "%s: %s", lua_tostring(L, 1), strerror(errno));
  else
    return 0;
}

/* Lifted from luadirent, which I wrote. (There may be other modules with the
   same name. I didn't check at the time.) */
static int f_stat(lua_State* L) {
  struct stat st;
  if(stat(luaL_checkstring(L, 1), &st)) {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return 2;
  }
  lua_newtable(L);
  lua_pushstring(L, "size");
  lua_pushnumber(L, st.st_size);
  lua_settable(L, -3);
  lua_pushstring(L, "mtime");
  lua_pushnumber(L, st.st_mtime);
  lua_settable(L, -3);
  lua_pushstring(L, "directory");
  lua_pushboolean(L, S_ISDIR(st.st_mode));
  lua_settable(L, -3);
  return 1;
}

static bool recursive_ckdir(lua_State* L, char* dir, const int* slashbuf) {
  assert(*slashbuf == 0 || dir[*slashbuf] == '/');
  // Check directory.
  struct stat st;
  if(!stat(dir, &st) && S_ISDIR(st.st_mode)) return true;
  // Directory is not present. Try making it.
  if(!mkdir(dir, 0777)) return true;
  // Directory could not be created.
  if(errno == ENOENT) {
    // Because a parent is not present.
    // If there is no parent, this is somewhat difficult to do anything about.
    if(*slashbuf == 0) return false;
    int ret;
    dir[*slashbuf] = 0;
    ret = recursive_ckdir(L, dir, slashbuf + 1);
    dir[*slashbuf] = '/';
    // If the parent could not be created, we definitely can't.
    if(ret == false) return false;
    // It all depends on mkdir now.
    return !mkdir(dir, 0777);
  }
  else {
    // Some other reason. Fail.
    return false;
  }
}

static int f_ckdir(lua_State* L) {
  const char* dir = luaL_checkstring(L, 1);
  assert(dir[0]);
  char* p; int* q;
  char* buf = (char*)amalloc(strlen(dir)+1);
  strcpy(buf, dir);
  // Calculate the number of slashes
  // Don't count any starting slash or terminating slash
  size_t slashcount = 0;
  for(p = buf + 1; *p; ++p)
    if(*p == '/' && p[1]) ++slashcount;
  int* slashbuf = (int*)amalloc((slashcount + 1) * sizeof(int));
  slashbuf[slashcount] = 0;
  for(p = buf + 1, q = slashbuf + slashcount - 1; *p; ++p) {
    if(*p == '/' && p[1]) *q-- = p - buf;
  }
  bool success = recursive_ckdir(L, buf, slashbuf);
  free((void*)slashbuf);
  free((void*)buf);
  if(!success) return luaL_error(L, "Could not create directory '%s'; check permissions.", dir);
  else return 0;
}

static int f_replace(lua_State* L) {
  const char* dest, *src;
  dest = luaL_checkstring(L, 1);
  src = luaL_checkstring(L, 2);
  int fd = open(src, O_WRONLY);
  if(fd < 0) return luaL_error(L, "couldn't open '%s' to sync caches: %s", src, strerror(errno));
  if(fsync(fd)) {
    int _e = errno;
    close(fd);
    errno = _e;
    return luaL_error(L, "couldn't sync caches on '%s': %s", src, strerror(errno));
  }
  close(fd);
  if(rename(src, dest))
    return luaL_error(L, "couldn't replace '%s' with '%s': %s", dest, src, strerror(errno));
  else
    return 0;
}

static int f_copy(lua_State* L) {
  const char* dest, *src;
  dest = luaL_checkstring(L, 1);
  src = luaL_checkstring(L, 2);
  FILE* out, *in;
  in = fopen(src, "rb");
  // If the source file does not exist, the destination file is not changed.
  if(!in) return 0;
  out = fopen(dest, "wb");
  if(!out) {
    int _e = errno;
    fclose(in);
    return luaL_error(L, "couldn't open '%s': %s", dest, strerror(_e));
  }
  do {
    char buf[2048];
    size_t read = fread(buf, 1, 2048, in);
    if(ferror(in)) {
      int _e = errno;
      fclose(in);
      fclose(out);
      remove(dest);
      return luaL_error(L, "reading '%s': %s", src, strerror(_e));
    }
    if(read > 0) {
      if(fwrite(buf, 1, read, out) < read) {
	int _e = errno;
	fclose(in);
	fclose(out);
	remove(dest);
	return luaL_error(L, "writing '%s': %s", dest, strerror(_e));
      }
    }
  } while(!feof(in));
  fclose(in);
  fclose(out);
  return 0;
}

static int f_parsecmdline(lua_State* L) {
  const char* cmdline = luaL_checkstring(L, 2);
  char* buf = (char*)malloc(strlen(cmdline));
  int argn = 1;
  const char* p;
  char* q = buf;
  for(p = cmdline; *p; ++p) {
    if(*p == '"') {
      for(++p; *p && *p != '"'; ++p) {
	if(*p == '\\')
	  ++p;
	if(*p)
	  *q++ = *p;
      }
    }
    else if(*p == '\'') {
      for(++p; *p && *p != '\''; ++p) {
	if(*p == '\\')
	  ++p;
	if(*p)
	  *q++ = *p;
      }
    }
    else if(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
      if(q != buf) {
	lua_pushlstring(L, buf, q - buf);
	lua_rawseti(L, 1, argn++);
	q = buf;
      }
    }
    else *q++ = *p;
  }
  if(q != buf) {
    lua_pushlstring(L, buf, q - buf);
    lua_rawseti(L, 1, argn++);    
  }
  free(buf);
  return 0;
}

static const luaL_Reg regs[] = {
  {"listfiles", f_listfiles},
  {"dlopen", f_dlopen},
  {"dlsym", f_dlsym},
  {"chdir", f_chdir},
  {"stat", f_stat},
  {"ckdir", f_ckdir},
  {"replace", f_replace},
  {"copy", f_copy},
  {"parse_command_line", f_parsecmdline},
  {NULL, NULL}, 
};

LUA_EXPORT int luaopen_subcritical_helper(lua_State* L) {
  lua_newtable(L);
  luaL_register(L, NULL, regs);
  lua_pushliteral(L, OS);
  lua_setfield(L, -2, "os");
  lua_pushliteral(L, SO_EXTENSION);
  lua_setfield(L, -2, "so_extension");
  return 1;
}
