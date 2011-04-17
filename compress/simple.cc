#include "subcritical/core.h"

#include <zlib.h>
#include <stdlib.h>
#include <lauxlib.h>

using namespace SubCritical;

SUBCRITICAL_UTILITY(Compress)(lua_State* L) throw() {
  size_t len;
  const char* string = luaL_checklstring(L,1,&len);
  int level = luaL_optinteger(L,2,6);
  if(level < 1 || level > 9)
    return luaL_error(L, "level must be between 1 and 9 inclusive");
  uLongf dstlen = compressBound(len);
  char* buf = (char*)malloc(dstlen);
  if(compress2((Bytef*)buf,&dstlen,(const Bytef*)string,len,level) != Z_OK) {
    free(buf);
    return luaL_error(L, "BAD");
  }
  lua_pushlstring(L, buf, dstlen);
  free(buf);
  return 1;
}

SUBCRITICAL_UTILITY(Uncompress)(lua_State* L) throw() {
  size_t len;
  const char* string = luaL_checklstring(L,1,&len);
  uLongf dstlen = compressBound(len);
  char* buf = (char*)malloc(dstlen);
  if(uncompress((Bytef*)buf,&dstlen,(const Bytef*)string,len) != Z_OK) {
    free(buf);
    return luaL_error(L, "BAD");
  }
  lua_pushlstring(L, buf, dstlen);
  free(buf);
  return 1;
}
