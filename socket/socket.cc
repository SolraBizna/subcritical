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
#include "socket.h"

#include <string.h>
#include <stdlib.h>

using namespace SubCritical;

const char* Socket::FailReasonToString(FailReason reason) throw() {
  switch(reason) {
  case Broken: return "connection broken";
  case NotReady: return "not ready";
  case PacketTooBig: return "packet too big";
  default: return "unknown error";
  }
}

static const struct ObjectMethod SocketMethods[] = {
  METHOD("GetAddressParts", &Socket::Lua_GetAddressParts),
  METHOD("SetAddressPart", &Socket::Lua_SetAddressPart),
  METHOD("ApplyAddress", &Socket::Lua_ApplyAddress),
  METHOD("SetBlocking", &Socket::Lua_SetBlocking),
  METHOD("GetPrintableAddress", &Socket::Lua_GetPrintableAddress),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(Socket, Object, SocketMethods);

static const struct ObjectMethod ListenSocketMethods[] = {
  METHOD("Accept", &ListenSocket::Lua_Accept),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(ListenSocket, Socket, ListenSocketMethods);

PROTOCOL_IMP_PLAIN(ListenSocketDgram, ListenSocket);

PROTOCOL_IMP_PLAIN(ListenSocketStream, ListenSocket);

ConnectedSocket::ConnectedSocket() throw() : buf(NULL), bufsz(0) {}

ConnectedSocket::~ConnectedSocket() {
  free(buf);
}

void* ConnectedSocket::CheckBuf(size_t size) throw(std::bad_alloc) {
  if(size < bufsz) return buf;
  else {
    void* nubuf = realloc(buf, size);
    if(!nubuf) throw std::bad_alloc();
    buf = nubuf;
    bufsz = size;
    return buf;
  }
}

void* ConnectedSocket::GetBuf(size_t& size) throw() {
  if(size < 256) return CheckBuf(256);
  else {
    size = bufsz;
    return buf;
  }
}

static const struct ObjectMethod CSockMethods[] = {
  METHOD("Receive", &ConnectedSocket::Lua_Receive),
  METHOD("Send", &ConnectedSocket::Lua_Send),
  NOMOREMETHODS(),
};
PROTOCOL_IMP(ConnectedSocket, Socket, CSockMethods)

int SocketDgram::Lua_Receive(lua_State* L) throw() {
  lua_Integer i = luaL_optinteger(L, 1, 1484); // 1484 is the maximum possible MTU for UDP over PPP; a more resilient value could be 65507, the maximum possible size for a UDP packet
  if(i <= 0 || i > 65535) return luaL_typerror(L, 1, "reasonable number (1-65535) or nil");
  void* buf = CheckBuf(i);
  FailReason error;
  size_t read = ReceiveDgram(buf, i, error);
  if(read == 0) {
    lua_pushnil(L);
    lua_pushstring(L, FailReasonToString(error));
    return 2;
  }
  else {
    lua_pushlstring(L, (const char*)buf, read);
    return 1;
  }
}

int SocketDgram::Lua_Send(lua_State* L) throw() {
  size_t size;
  const char* buf = luaL_checklstring(L, 1, &size);
  FailReason error;
  if(!SendDgram(buf, size, error)) {
    lua_pushnil(L);
    lua_pushstring(L, FailReasonToString(error));
    return 2;
  }
  else {
    lua_pushboolean(L, 1);
    return 1;
  }
}

PROTOCOL_IMP_PLAIN(SocketDgram, ConnectedSocket);

int SocketStream::Lua_Receive(lua_State* L) throw() {
  lua_Integer i = luaL_checkinteger(L, 1);
  if(i <= 0 || i > 65535) return luaL_typerror(L, 1, "reasonable number (1-65535)");
  void* buf = CheckBuf(i);
  FailReason error;
  size_t read = ReceiveBytes(buf, i, error);
  if(read == 0) {
    lua_pushnil(L);
    lua_pushstring(L, FailReasonToString(error));
    return 2;
  }
  else {
    lua_pushlstring(L, (const char*)buf, read);
    return 1;
  }
}

int SocketStream::Lua_Send(lua_State* L) throw() {
  size_t size;
  const char* buf = luaL_checklstring(L, 1, &size);
  FailReason error;
  size_t ssize = SendBytes(buf, size, error);
  if(!ssize) {
    lua_pushnil(L);
    lua_pushstring(L, FailReasonToString(error));
    return 2;
  }
  else {
    lua_pushnumber(L, ssize);
    return 1;
  }
}

PROTOCOL_IMP_PLAIN(SocketStream, ConnectedSocket);

