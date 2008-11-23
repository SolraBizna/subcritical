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
#include "ipsocket.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

using namespace SubCritical;

IPSocket::IPSocket(lua_State* L, int sock, const struct sockaddr_in& addr) throw() : addrhost(NULL), addrhostset(false), addrportset(false), bound(true), addr(addr), sock(sock) {
  SetBlocking(L, false); // will leak memory on failure
}

IPSocket::IPSocket(int type, int protocol) throw(std::bad_alloc, int) : addrhost(NULL), addrhostset(false), addrportset(false), bound(false) {
  addrhost = (char*)malloc(MAXHOST+1);
  if(!addrhost) throw std::bad_alloc();
  sock = socket(PF_INET, type, protocol);
  if(sock < 0) {
    free(addrhost);
    addrhost = NULL;
    throw errno;
  }
}

IPSocket::~IPSocket() {
  if(addrhost) free(addrhost);
  if(sock >= 0) close(sock);
}

int IPSocket::Lua_SubGetAddressParts(lua_State* L) throw() {
  if(bound) return luaL_error(L, "GetAddressParts called on an already-bound socket");
  lua_createtable(L, 0, 2);
  if(addrhostset) lua_pushstring(L, addrhost);
  else lua_pushboolean(L, 0);
  lua_setfield(L, -2, "Host");
  if(addrportset) lua_pushnumber(L, addrport);
  else lua_pushboolean(L, 0);
  lua_setfield(L, -2, "Port");
  return 1;
}

static const char* parts[] = {"Host", "Port", NULL};

int IPSocket::Lua_SubSetAddressPart(lua_State* L) throw() {
  switch(luaL_checkoption(L, 1, NULL, parts)) {
  case 0:
    {
      size_t size;
      const char* str = luaL_checklstring(L, 2, &size);
      if(size > MAXHOST) return luaL_error(L, "host name may not exceed %i octets", MAXHOST);
      if(str[0] == '*' && str[1] == 0)
	memcpy(addrhost, "0.0.0.0", 8);
      else
	memcpy(addrhost, str, size+1);
      addrhostset = true;
    }
    break;
  case 1:
    {
      int port;
      port = luaL_checkinteger(L, 2);
      if(port < 0 || port > 65535) return luaL_typerror(L, 2, "integer between 0 and 65535 inclusive");
      addrport = port;
      addrportset = true;
    }
    break;
  }
  return 0;
}

int IPSocket::SetBlocking(lua_State* L, bool blocking) throw() {
  if(!bound) return luaL_error(L, "Not yet bound, we don't have a concept of blocking.");
  if(blocking) {
    if(fcntl(sock, F_SETFL, 0)) {
      return luaL_error(L, "%s", strerror(errno));
    }
  }
  else {
    if(fcntl(sock, F_SETFL, O_NONBLOCK)) {
      return luaL_error(L, "%s", strerror(errno));
    }
  }
  return 0;
}

int IPSocket::Lua_SubSetBlocking(lua_State* L) throw() {
  bool blocking = lua_toboolean(L, 1);
  return SetBlocking(L, blocking);
}

int IPSocket::Lua_SubGetPrintableAddress(lua_State* L) throw() {
  if(!bound) return luaL_error(L, "Not yet bound, we don't have an address.");
  if(addr.sin_addr.s_addr != INADDR_ANY) {
    in_addr_t ip = htonl(addr.sin_addr.s_addr);
    in_port_t port = htons(addr.sin_port);
    lua_pushfstring(L, "%i.%i.%i.%i:%i", (ip >> 24), (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, port);
  }
  else {
    in_port_t port = htons(addr.sin_port);
    lua_pushfstring(L, "*:%i", port);
  }
  return 1;
}

int IPSocket::Lua_SubApplyAddress(lua_State* L) throw() {
  if(bound) return luaL_error(L, "Already bound");
  if(!addrhostset || !addrportset) return luaL_error(L, "Not enough information given, for an IP socket you must at least provide Host and Port.");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(addrport);
  struct hostent* host = gethostbyname(addrhost);
  int n = 0;
  if(!host || !host->h_addr_list[n]) {
    lua_pushnil(L);
    if(h_errno)
      lua_pushstring(L, hstrerror(h_errno));
    else
      lua_pushstring(L, "no IPv4 address records for host");
    return 2;
  }
  addr.sin_addr.s_addr = *(in_addr_t*)(host->h_addr_list[n]);
  return 0;
}

/*
  if(listen) {
    if(bind(sock, (struct sockaddr*)addr, sizeof(struct sockaddr_in))) {
      lua_pushnil(L);
      switch(errno) {
      case EACCESS: lua_pushstring(L, "cannot bind to privileged port"); break;
      default: lua_pushstring(L, strerror(errno)); break;
      }
      return 2;
    }
  }
  else {
    if(connect(sock, (struct sockaddr*)addr, sizeof(struct sockaddr_in))) {
      lua_pushnil(L);
      switch(errno) {
      case ECONNREFUSED: lua_pushstring(L, "connection refused"); break;
      default: lua_pushstring(L, strerror(errno)); break;
      }
      return 2;
    }
  }
  free(addrhost);
  addrhost = NULL;
*/
