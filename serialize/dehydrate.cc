/*
  This source file is part of the SubCritical core package set.
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
#include "serialize.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <map>

#define MAX_ID (0x0007FFFF)
#define MAX_STRING (0x1FFFFFFF)
#define MAX_TABLE (0x003FFFFF)

using namespace SubCritical;

typedef std::map<const void*, uint32_t> idmap;

class LOCAL SuperBuffer {
 public:
  SuperBuffer(lua_State* L) throw(std::bad_alloc) : L(L), bigbuf((unsigned char*)malloc(1024)), bigsize(1024), realsize(0) {
    if(!bigbuf) throw std::bad_alloc();
  }
  ~SuperBuffer() {
    lua_pushlstring(L, (const char*)bigbuf, realsize);
    free(bigbuf);
  }
  void Write(const unsigned char* buf, size_t size) throw(std::bad_alloc) {
    if(realsize + size > bigsize) {
      size_t newsize = bigsize;
      do newsize = newsize << 1;
      while(realsize + size > newsize);
      // this can overflow in seriously screwed up circumstances
      unsigned char* newbigbuf = (unsigned char*)realloc(bigbuf, newsize);
      if(!newbigbuf) throw std::bad_alloc();
      bigbuf = newbigbuf;
      bigsize = newsize;
    }
    memcpy(bigbuf + realsize, buf, size);
    realsize += size;
  }
 private:
  lua_State* L;
  unsigned char* bigbuf;
  size_t bigsize, realsize;
};

static void WriteValue(lua_State* L, SuperBuffer& output, idmap& ids, uint32_t& id);

static void WriteAtom(SuperBuffer& output, CelduinAtom atom) {
  unsigned char out[1] = {atom};
  output.Write(out, 1);
}

static void WriteID(SuperBuffer& output, CelduinAtom atom, uint32_t id) {
  unsigned char out[3]; // an ID can't take more than 3 bytes
  if(id < 16) {
    out[0] = atom | id;
    output.Write(out, 1); 
  }
  else if(id < 2048) {
    out[0] = atom | (id >> 7) | 0x10;
    out[1] = id & 127;\
    output.Write(out, 2);
  }
  else if(id <= MAX_ID) {
    out[0] = atom | (id >> 15) | 0x10;
    out[1] = ((id >> 8) & 127) | 0x80;
    out[2] = id & 255;
    output.Write(out, 3);
  }
  else throw "Too many IDs!";
}

static void WriteString(SuperBuffer& output, idmap& ids, uint32_t& id, const char* string, size_t length) {
  if(ids[(const void*)string]) WriteID(output, String, ids[(const void*)string]);
  else {
    ids[(const void*)string] = id;
    WriteID(output, String, id);
    ++id;
    {
      unsigned char out[4];
      if(length < 128) {
	out[0] = length;
	output.Write(out, 1);
      }
      else if(length < 16384) {
	out[0] = (length >> 7) | 0x80;
	out[1] = length & 127;
	output.Write(out, 2);
      }
      else if(length < 2097152) {
	out[0] = (length >> 14) | 0x80;
	out[1] = ((length >> 7) & 127) | 0x80;
	out[2] = length & 127;
	output.Write(out, 3);
      }
      else if(length <= MAX_STRING) {
	out[0] = (length >> 22) | 0x80;
	out[1] = ((length >> 15) & 127) | 0x80;
	out[2] = ((length >> 8) & 127) | 0x80;
	out[3] = length & 255;
	output.Write(out, 4);
      }
      else throw "String too long!";
    }
    output.Write((const unsigned char*)string, length);
  }
}

static void WriteTable(lua_State* L, SuperBuffer& output, idmap& ids, uint32_t& id) {
  const void* table = lua_topointer(L, -1);
  if(ids[table]) WriteID(output, Table, ids[table]);
  else {
    lua_checkstack(L, 4);
    ids[table] = id;
    WriteID(output, Table, id);
    ++id;
    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
      lua_pushvalue(L, -2);
      WriteValue(L, output, ids, id);
      WriteValue(L, output, ids, id);
    }
    WriteAtom(output, EndTable);
  }
}

static void WriteValue(lua_State* L, SuperBuffer& output, idmap& ids, uint32_t& id) {
  switch(lua_type(L, -1)) {
  default: throw "Celduin can only serialize nils, booleans, strings, tables, and numbers.";
  case LUA_TNIL: WriteAtom(output, Nil); break;
  case LUA_TBOOLEAN: WriteAtom(output, lua_toboolean(L,-1) ? True : False); break;
  case LUA_TSTRING:
    {
      size_t length;
      const char* string = lua_tolstring(L, -1, &length);
      WriteString(output, ids, id, string, length);
      break;
    }
  case LUA_TTABLE: WriteTable(L, output, ids, id); break;
  case LUA_TNUMBER:
    {
      double no = (double)lua_tonumber(L, -1);
      if(no == 0) {
	WriteAtom(output, Zero);
	break;
      }
      else if(no >= -32768 && no <= 32799) {
	double nof = floor(no);
	if(nof == no) {
	  if(nof >= 1 && nof <= 31)
	    WriteAtom(output, (CelduinAtom)(Number | (int)nof));
	  else {
	    WriteAtom(output, Int16);
	    int16_t x;
	    if(nof >= 0) x = (int16_t)(nof-32);
	    else x = (int16_t)nof;
	    union {
	      uint16_t u;
	      unsigned char raw[2];
	    } ux;
	    ux.u = Swap16_BE((uint16_t)x);
	    output.Write(ux.raw, sizeof(ux));
	  }
	  break;
	}
      }
      WriteAtom(output, Number);
      union {
	double n;
	unsigned char raw[sizeof(double)];
      } in;
      in.n = no;
      if(float_little_endian) {
	unsigned char out[sizeof(double)];
	for(int i = 0; i < (int)sizeof(double); ++i) {
	  out[sizeof(double)-i-1] = in.raw[i];
	}
	output.Write(out, sizeof(double));
      }
      else output.Write(in.raw, sizeof(double));
    }
    break;
  }
  lua_pop(L, 1);
}

SUBCRITICAL_UTILITY(Dehydrate)(lua_State* L) {
  if(lua_gettop(L) != 1)
    return luaL_error(L, "Usage: Dehydrate(nil/boolean/number/string/table)");
  try {
    SuperBuffer output(L);
    uint32_t id = 0;
    idmap ids;
    output.Write((const unsigned char*)"CEL", 3);
    WriteValue(L, output, ids, id);
  }
  catch(std::bad_alloc& e) {
    return luaL_error(L, "Memory allocation failure");
  }
  catch(const char* e) {
    return luaL_error(L, "%s", e);
  }
  return 1;
}
