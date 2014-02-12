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
#include "ipsocket.h"
#ifndef __WIN32__
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __WIN32__
#define socketoption(sock, lev, name, val, len) setsockopt(sock, lev, name, (const char*) val, len)
#else
#define socketoption(sock, lev, name, val, len) setsockopt(sock, lev, name, (int*) val, len)
#include <netinet/tcp.h>
#endif

using namespace SubCritical;

#ifdef __WIN32__
static WSADATA wsaData;
LUA_EXPORT int Init_ipsocket(lua_State* L) {
  int fail;
  if((fail = WSAStartup(MAKEWORD(1,1),&wsaData)))
    return luaL_error(L, "WSAStartup fail: %d\n", fail);
  fprintf(stderr, "WinSock version %i.%i in use (%i.%i max)\nDescription: %s\nStatus: %s\n", wsaData.wVersion & 255, wsaData.wVersion >> 8, wsaData.wHighVersion & 255, wsaData.wHighVersion >> 8, wsaData.szDescription, wsaData.szSystemStatus);
  return 0;
}
#else
LUA_EXPORT int Init_ipsocket(lua_State* L) {
  signal(SIGPIPE, SIG_IGN);
  return 0;
}
#endif

IPSocket::IPSocket(lua_State* L, SOCKET sock, const struct sockaddr_in& addr) throw() : addrhost(NULL), addrhostset(false), addrportset(false), bound(true), addr(addr), sock(sock) {
  SetBlocking(L, false); // will leak memory on failure
}

IPSocket::IPSocket(int type, int protocol) throw(std::bad_alloc, int) : addrhost(NULL), addrhostset(false), addrportset(false), bound(false) {
  addrhost = (char*)malloc(MAXHOST+1);
  if(!addrhost) throw std::bad_alloc();
  sock = socket(AF_INET, type, protocol);
#ifdef __WIN32__
  if(sock == INVALID_SOCKET) {
#else
  if(sock < 0) {
#endif
    free(addrhost);
    addrhost = NULL;
    throw errno;
  }
  int one = 1;
  socketoption(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}

IPSocket::~IPSocket() {
  if(addrhost) free(addrhost);
#ifdef __WIN32__
  if(sock != INVALID_SOCKET) closesocket(sock);
#else
  if(sock >= 0) close(sock);
#endif
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
      if(size > MAXHOST) return luaL_error(L, "host name may not exceed %d octets", MAXHOST);
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
  this->blocking = blocking;
#ifdef __WIN32__
  u_long nonblocking = !blocking;
  if(ioctlsocket(sock, FIONBIO, &nonblocking) < 0) {
#else
  if(fcntl(sock, F_SETFL, blocking ? 0 : O_NONBLOCK) < 0) {
#endif
    return luaL_error(L, "%s", ErrorToString(errno));
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
    uint32_t ip = htonl(addr.sin_addr.s_addr);
    uint16_t port = htons(addr.sin_port);
    lua_pushfstring(L, "%d.%d.%d.%d:%d", (ip >> 24), (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, port);
  }
  else {
    uint16_t port = htons(addr.sin_port);
    lua_pushfstring(L, "*:%d", port);
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
    int h_err = h_errno;
    if(h_err)
#ifdef __WIN32__
      lua_pushstring(L, ErrorToString(h_err));
#else
      lua_pushstring(L, hstrerror(h_err));
#endif
    else
      lua_pushstring(L, "no IPv4 address records for host");
    return 2;
  }
  addr.sin_addr.s_addr = *(uint32_t*)(host->h_addr_list[n]);
  return 0;
}

#ifdef __WIN32__
const char* IP::WinSockErrorToString(int err) {
  static char ret[512];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 0, ret, 512, 0);
  return ret;
}
#endif

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
