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

#include "ipsocket.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __WIN32__
#define EMSGSIZE WSAEMSGSIZE
#endif

using namespace SubCritical;
using namespace IP;
using namespace std;

IPSOCKET_SUBCLASS_IMP(UDPSocket);

UDPSocket::UDPSocket() throw(std::bad_alloc,int) : IPSocket(SOCK_DGRAM, IPPROTO_UDP) {}

int UDPSocket::Lua_ApplyAddress(lua_State* L) throw() {
  { int ret; if((ret = Lua_SubApplyAddress(L))) return ret; }
  if(connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
    lua_pushnil(L);
    lua_pushstring(L, ErrorToString(errno));
    return 2;
  }
  free(addrhost);
  addrhost = NULL;
  bound = true;
  SetBlocking(L, false);
  lua_pushboolean(L, 1);
  return 1;
}

size_t UDPSocket::ReceiveDgram(void* buf, size_t max_size, FailReason& error) throw() {
  ssize_t got = recv(sock, (char*)buf, max_size, 0);
  if(got <= 0) {
    switch(errno) {
    case EINTR:
    case EAGAIN: error = NotReady; return 0;
    default: error = Broken; return 0;
    }
  }
  return (size_t)got;
}

bool UDPSocket::SendDgram(const void* buf, size_t max_size, FailReason& error) throw() {
  ssize_t sent = send(sock, (const char*)buf, max_size, 0);
  if(sent <= 0) {
    switch(errno) {
    case EMSGSIZE: error = PacketTooBig; return false;
    case EINTR:
    case EAGAIN: error = NotReady; return false;
    default: error = Broken; return false;
    }
  }
  return true;
}

PROTOCOL_IMP_PLAIN(UDPSocket, SocketDgram);

SUBCRITICAL_CONSTRUCTOR(UDPSocket)(lua_State* L) {
  try {
    (new UDPSocket())->Push(L);
    return 1;
  }
  catch(const std::bad_alloc& e) {
    return luaL_error(L, "std::bad_alloc caught");
  }
  catch(int e) {
    return luaL_error(L, "%s", ErrorToString(e));
  }
}

IPSOCKET_SUBCLASS_IMP(UDPListenSocket);

static char gate;
#define lua_pushgate(L) lua_pushlightuserdata(L, &gate)

UDPListenSocket::UDPListenSocket(lua_State* L) throw(std::bad_alloc,int) : IPSocket(SOCK_DGRAM, IPPROTO_UDP), L(L) {
  // aside from the intended effect, this will ensure that we don't leak if a
  // Lua error occurs in the rest of the function
  this->Push(L);
  // Ensure that a weak reference to ourselves is planted in reg[gate], so that
  // UDPSlaveSockets can plant references to us in the registry to keep us from
  // being collected if we have slaves in the field.
  lua_pushgate(L);
  lua_gettable(L, LUA_REGISTRYINDEX);
  if(lua_isnil(L, -1)) {
    // Ensure that reg[gate] exists, so that weak references to
    // UDPListenSockets can be planted in it.
    lua_pop(L, 1);
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushliteral(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);
    lua_pushgate(L);
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);
  }
  lua_pushlightuserdata(L, this);
  lua_pushvalue(L, -3);
  lua_settable(L, -3);
  lua_pop(L, 1);
}

UDPListenSocket::~UDPListenSocket() {
  lua_pushgate(L);
  lua_gettable(L, LUA_REGISTRYINDEX);
  lua_pushlightuserdata(L, this);
  lua_pushnil(L);
  lua_settable(L, -3);
  lua_pop(L, 1);
  for(deque<DeferredDgram>::iterator cur = recruits.begin(); cur != recruits.end(); ++cur) {
    free((*cur).packet);
  }
  assert(slaves.empty());
}

void UDPListenSocket::Massage() throw(std::bad_alloc) {
  fd_set rfd;
  struct timeval timeout = {0,0};
  FD_ZERO(&rfd);
  FD_SET(sock, &rfd);
  while(select(sock+1, &rfd, NULL, NULL, &timeout) > 0) {
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    ssize_t size = recvfrom(sock, (char*)buf, UDP_MTU, 0, (struct sockaddr*)&addr, &addrlen);
    if(size <= 0) continue;
    addr_port_t source = SOCKADDR_TO_INT(addr);
    map<addr_port_t, UDPSlaveSocket*>::iterator slave = slaves.find(source);
    void* nubuf = malloc(size);
    if(!nubuf) throw std::bad_alloc();
    memcpy(nubuf, buf, size);
    if(slave == slaves.end())
      recruits.push_back(DeferredDgram(size, nubuf, source));
    else
      (*slave).second->packets.push_back(DeferredMessage(size, nubuf));
  }
}

int UDPListenSocket::Lua_Accept(lua_State* L) throw() {
  Massage();
  if(recruits.empty()) {
    if(blocking) {
      struct timeval timeout = {120,0};
      fd_set rfd;
      FD_ZERO(&rfd);
      FD_SET(sock, &rfd);
      while(select(sock+1, &rfd, NULL, NULL, &timeout) <= 0) {
	timeout.tv_sec = 120;
	FD_ZERO(&rfd);
	FD_SET(sock, &rfd);
      }
      return Lua_Accept(L);
    }
    else return 0;
  }
  deque<DeferredDgram>::iterator recruit = recruits.begin();
  UDPSlaveSocket* slave = new UDPSlaveSocket(this, (*recruit).source);
  slave->Push(L);
  // This... is... awful
  bool deleted;
  do {
    deleted = false;
    for(recruit = recruits.begin(); recruit != recruits.end(); ++recruit) {
      if((*recruit).source == slave->addr) {
	slave->packets.push_back(*recruit);
	deleted = true;
	recruits.erase(recruit);
	break;
      }
    }
  } while(deleted);
  slaves[slave->addr] = slave;
  return 1;
}

int UDPListenSocket::Lua_ApplyAddress(lua_State* L) throw() {
  { int ret; if((ret = Lua_SubApplyAddress(L))) return ret; }
  if(bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
    lua_pushnil(L);
    lua_pushstring(L, ErrorToString(errno));
    return 2;
  }
  free(addrhost);
  addrhost = NULL;
  bound = true;
  SetBlocking(L, false);
  lua_pushboolean(L, 1);
  return 1;
}

PROTOCOL_IMP_PLAIN(UDPListenSocket, ListenSocketDgram);

SUBCRITICAL_CONSTRUCTOR(UDPListenSocket)(lua_State* L) {
  try {
    // the UDPListenSocket constructor pushes itself onto the stack.
    (void)new UDPListenSocket(L);
    return 1;
  }
  catch(const std::bad_alloc& e) {
    return luaL_error(L, "std::bad_alloc caught");
  }
  catch(int e) {
    return luaL_error(L, "%s", ErrorToString(e));
  }
}

UDPSlaveSocket::UDPSlaveSocket(UDPListenSocket* master, addr_port_t addr) throw() : addr(addr), master(master) {
  lua_State* L = master->L;
  lua_pushgate(L);
  lua_gettable(L, LUA_REGISTRYINDEX);
  assert(lua_istable(L, -1));
  lua_pushlightuserdata(L, this);
  lua_pushlightuserdata(L, master);
  lua_gettable(L, -3);
  lua_settable(L, LUA_REGISTRYINDEX);
  lua_pop(L, 1);
}

UDPSlaveSocket::~UDPSlaveSocket() {
  lua_State* L = master->L;
  lua_pushgate(L);
  lua_gettable(L, LUA_REGISTRYINDEX);
  assert(lua_istable(L, -1));
  lua_pushlightuserdata(L, this);
  lua_pushnil(L);
  lua_settable(L, -3);
  lua_pop(L, 1);
  for(deque<DeferredMessage>::iterator cur = packets.begin(); cur != packets.end(); ++cur) {
    free((*cur).packet);
  }
}

bool UDPSlaveSocket::SendDgram(const void* buf, size_t max_size, FailReason& error) throw() {
  ssize_t sent = send(master->sock, (const char*)buf, max_size, 0);
  if(sent <= 0) {
    switch(errno) {
    case EMSGSIZE: error = PacketTooBig; return false;
    case EINTR:
    case EAGAIN:
      if(master->blocking) {
	struct timeval timeout = {120,0};
	fd_set wfd;
	FD_ZERO(&wfd);
	FD_SET(master->sock, &wfd);
	while(select(master->sock+1, NULL, &wfd, NULL, &timeout) <= 0) {
	  timeout.tv_sec = 120;
	  FD_ZERO(&wfd);
	  FD_SET(master->sock, &wfd);
	}	
	return SendDgram(buf, max_size, error);
      }
      error = NotReady;
      return false;
    default: error = Broken; return false;
    }
  }
  return true;
}

int UDPSlaveSocket::Lua_ApplyAddress(lua_State* L) throw() {
  return luaL_error(L, "This is a UDPSlaveSocket, it is always connected");
}
int UDPSlaveSocket::Lua_GetAddressParts(lua_State* L) throw() {
  return luaL_error(L, "This is a UDPSlaveSocket, it is always connected");
}
int UDPSlaveSocket::Lua_SetAddressPart(lua_State* L) throw() {
  return luaL_error(L, "This is a UDPSlaveSocket, it is always connected");
}
int UDPSlaveSocket::Lua_SetBlocking(lua_State* L) throw() {
  return luaL_error(L, "A UDPSlaveSocket's blocking status is slaved to its master UDPListenSocket");
}

int UDPSlaveSocket::Lua_GetPrintableAddress(lua_State* L) throw() {
    uint32_t ip = htonl(addr);
    uint16_t port = htons(addr >> 32);
  lua_pushfstring(L, "%d.%d.%d.%d:%d", ((int)(ip >> 24) & 255), ((int)(ip >> 16) & 255), ((int)(ip >> 8) & 255), ((int)ip & 255), port);
  return 1;
}

size_t UDPSlaveSocket::ReceiveDgram(void* buf, size_t max_size, FailReason& error) throw() {
  if(packets.empty()) {
    master->Massage();
    if(master->blocking) {
      struct timeval timeout = {120,0};
      fd_set rfd;
      FD_ZERO(&rfd);
      FD_SET(master->sock, &rfd);
      while(select(master->sock+1, &rfd, NULL, NULL, &timeout) <= 0) {
	timeout.tv_sec = 120;
	FD_ZERO(&rfd);
	FD_SET(master->sock, &rfd);
      }
      return ReceiveDgram(buf, max_size, error);
    }
    else if(packets.empty()) {
      error = NotReady;
      return 0;
    }
  }
  DeferredMessage& message = *packets.begin();
  size_t size = message.size;
  if(size > max_size) {
    error = PacketTooBig;
    return 0;
  }
  memcpy(buf, message.packet, size);
  free(message.packet);
  packets.pop_front();
  return size;
}
