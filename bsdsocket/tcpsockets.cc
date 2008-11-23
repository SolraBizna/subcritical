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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

using namespace SubCritical;

IPSOCKET_SUBCLASS_IMP(TCPSocket);
IPSOCKET_SUBCLASS_IMP(TCPListenSocket);

TCPSocket::TCPSocket() throw(std::bad_alloc,int) : IPSocket(SOCK_STREAM, IPPROTO_TCP) {}

TCPSocket::TCPSocket(lua_State* L, int sock, const struct sockaddr_in& addr) throw() : IPSocket(L, sock, addr) {}

int TCPSocket::Lua_ApplyAddress(lua_State* L) throw() {
  { int ret; if((ret = Lua_SubApplyAddress(L))) return ret; }
  if(connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return 2;
  }
  free(addrhost);
  addrhost = NULL;
  bound = true;
  SetBlocking(L, false);
  lua_pushboolean(L, 1);
  return 1;
}

size_t TCPSocket::ReceiveBytes(void* buf, size_t bytes, FailReason& error) throw() {
  ssize_t did_read = read(sock, buf, bytes);
  if(did_read <= 0) {
    did_read = 0;
    switch(errno) {
    case EAGAIN: error = NotReady; break;
    default:
    case EPIPE:
    case ETIMEDOUT: error = Broken; break;
    }
  }
  return did_read;
}

size_t TCPSocket::SendBytes(const void* buf, size_t bytes, FailReason& error) throw() {
  const char* p = (const char*)buf;
  size_t rem = bytes;
  while(rem > 0) {
    ssize_t did_write = write(sock, p, rem);
    if(did_write <= 0) {
      did_write = 0;
      switch(errno) {
      case EAGAIN: error = NotReady; return bytes - rem;
      default:
      case EPIPE:
      case ETIMEDOUT: error = Broken; return false;
      }
    }
    rem = rem - did_write;
  }
  return bytes;
}

PROTOCOL_IMP_PLAIN(TCPSocket, SocketStream);

SUBCRITICAL_CONSTRUCTOR(TCPSocket)(lua_State* L) {
  try {
    (new TCPSocket())->Push(L);
    return 1;
  }
  catch(const std::bad_alloc& e) {
    return luaL_error(L, "std::bad_alloc caught");
  }
  catch(int e) {
    return luaL_error(L, "%s", strerror(e));
  }
}

TCPListenSocket::TCPListenSocket() throw(std::bad_alloc,int) : IPSocket(SOCK_STREAM, IPPROTO_TCP) {}

int TCPListenSocket::Lua_ApplyAddress(lua_State* L) throw() {
  { int ret; if((ret = Lua_SubApplyAddress(L))) return ret; }
  if(bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return 2;
  }
  free(addrhost);
  addrhost = NULL;
  bound = true;
  if(listen(sock, 3))
    return luaL_error(L, "listen: %s", strerror(errno));
  SetBlocking(L, false);
  lua_pushboolean(L, 1);
  return 1;
}

int TCPListenSocket::Lua_Accept(lua_State* L) throw() {
  struct sockaddr_in nuaddr;
  socklen_t nuaddr_len = sizeof(nuaddr);
  int nusock = accept(sock, (struct sockaddr*)&nuaddr, &nuaddr_len);
  if(nusock) {
    (new TCPSocket(L, nusock, nuaddr))->Push(L);
    return 1;
  }
  else return 0;
}

PROTOCOL_IMP_PLAIN(TCPListenSocket, ListenSocketStream);

SUBCRITICAL_CONSTRUCTOR(TCPListenSocket)(lua_State* L) {
  try {
    (new TCPListenSocket())->Push(L);
    return 1;
  }
  catch(const std::bad_alloc& e) {
    return luaL_error(L, "std::bad_alloc caught");
  }
  catch(int e) {
    return luaL_error(L, "%s", strerror(e));
  }
}
