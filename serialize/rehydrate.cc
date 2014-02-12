/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2014 Solra Bizna.

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
#include "serialize.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <map>

using namespace SubCritical;

typedef std::map<uint32_t, const void*> idmap;

#define ck_length(rem) if(length < rem) throw "this celduin has been truncated"
#define next_byte() do { ++in; --length; ck_length(1); } while(0)
#define soft_next_byte() do { ++in; --length; } while(0)

static void PushId(lua_State* L, const void* id) {
  lua_pushlightuserdata(L, (void*)id);
  lua_gettable(L, 2);
}

static uint32_t ReadId(const unsigned char*& in, size_t& length) {
  uint32_t ret;
  if(*in & 0x10) {
    ret = (*in & 15) << 7;
    next_byte();
    if(*in & 0x80) {
      ret = (ret | (*in & 127)) << 8;
      next_byte();
      ret = ret | *in;
    }
    else ret = ret | *in;
  }
  else ret = *in & 0x1F;
  soft_next_byte();
  return ret;
}

static void ReadValue(lua_State* L, const unsigned char*& in, size_t& length, idmap& ids) {
  ck_length(1);
  switch(*in & BigMask) {
  case String:
    {
      uint32_t id = ReadId(in, length);
      if(ids[id]) PushId(L, ids[id]);
      else {
	size_t strlen;
	ck_length(1);
	if(*in & 0x80) {
	  strlen = (*in & 127) << 7;
	  next_byte();
	  if(*in & 0x80) {
	    strlen = (strlen | (*in & 127)) << 7;
	    next_byte();
	    if(*in & 0x80) {
	      strlen = (strlen | (*in & 127)) << 7;
	      next_byte();
	      strlen |= *in;
	    }
	    else strlen |= *in;
	  }
	  else strlen |= *in;
	}
	else strlen = *in;
	soft_next_byte();
	ck_length(strlen);
	lua_pushlstring(L, (const char*)in, strlen);
	lua_pushlightuserdata(L, (void*)lua_tostring(L, -1));
	ids[id] = lua_topointer(L, -1);
	lua_pushvalue(L, -2);
	lua_settable(L, 2);
	in += strlen;
	length -= strlen;
      }
    }
    break;
  case Table:
    {
      uint32_t id = ReadId(in, length);
      if(ids[id]) PushId(L, ids[id]);
      else {
	lua_checkstack(L, 4);
	lua_newtable(L);
	const void* table = lua_topointer(L, -1);
	lua_pushlightuserdata(L, (void*)table);
	ids[id] = table;
	lua_pushvalue(L, -2);
	lua_settable(L, 2);
	while(*in != EndTable) {
	  ReadValue(L, in, length, ids);
	  ReadValue(L, in, length, ids);
	  lua_settable(L, -3);
	}
	soft_next_byte();
      }
    }
    break;
  case Number:
    if(*in & 0x1F) {
      lua_pushinteger(L, *in & 0x1F);
      soft_next_byte();
    }
    else if(float_little_endian) {
      soft_next_byte();
      union {
	double d;
	unsigned char raw[sizeof(double)];
      } convert;
      ck_length(sizeof(double));
      for(int i = 0; i < (int)sizeof(double); ++i) {
	convert.raw[sizeof(double)-i-1] = *in++;
      }
      lua_pushnumber(L, (lua_Number)convert.d);
    }
    else {
      soft_next_byte();
      ck_length(sizeof(double));
      lua_pushnumber(L, (lua_Number)*((double*)in));
      in += sizeof(double);
    }
    break;
  default:
    switch(*in) {
    default: throw "unknown atom, this celduin is corrupted";
    case Nil: lua_pushnil(L); break;
    case False: lua_pushboolean(L, 0); break;
    case True: lua_pushboolean(L, 1); break;
    case Zero: lua_pushnumber(L, 0.0); break;
    case Int16: {
      next_byte();
      uint16_t ux = *in << 8;
      next_byte();
      ux |= *in;
      int16_t x = (int16_t)ux;
      if(x >= 0) lua_pushnumber(L, x + 32);
      else lua_pushnumber(L, x);
    } break;
    case EndTable: throw "EndTable atom at inappropriate place in stream";
    }
    //++in;
    soft_next_byte();
    break;
  }
}

SUBCRITICAL_UTILITY(Rehydrate)(lua_State* L) {
  if(lua_gettop(L) != 1)
    return luaL_error(L, "Usage: Rehydrate(string)");
  size_t length;
  const unsigned char* in = (const unsigned char*)luaL_checklstring(L, 1, &length);
  try {
    if(length < 4 || memcmp(in, "CEL", 3))
      throw "string too short or magic number wrong";
    length -= 3;
    in += 3;
    lua_newtable(L);
    idmap ids;
    ReadValue(L, in, length, ids);
    return 1;
  }
  catch(const char* e) {
    return luaL_error(L, "format error: %s", e);
  }
}
