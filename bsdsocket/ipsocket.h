// -*- c++ -*-
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

#ifndef _SUBCRITICAL_BSD_IPSOCKET_H
#define _SUBCRITICAL_BSD_IPSOCKET_H

#ifndef IPSOCKET_IS_BSD
#define IPSOCKET_IS_BSD 1
#define IPSOCKET_IS_WINSOCK 0
#endif

#include "subcritical/socket.h"
#include <netinet/in.h>
#include <netdb.h>

#include <new>
#include <deque>
#include <map>

#define MAXHOST NI_MAXHOST

namespace IP {
  typedef uint64_t addr_port_t;
#define SOCKADDR_TO_INT(addr) ((addr).sin_addr.s_addr | ((addr_port_t)(addr).sin_port << 32))
  struct LOCAL DeferredDgram {
    inline DeferredDgram(size_t size, void* packet, addr_port_t source) : size(size), packet(packet), source(source) {}
    size_t size;
    void* packet;
    addr_port_t source;
  };
  struct LOCAL DeferredMessage {
    inline DeferredMessage(size_t size, void* packet) : size(size), packet(packet) {}
    inline DeferredMessage(const DeferredDgram& dgram) : size(dgram.size), packet(dgram.packet) {}
    size_t size;
    void* packet;
  };
}

namespace SubCritical {
  class LOCAL IPSocket {
  public:
    IPSocket(int, int) throw(std::bad_alloc,int);
    IPSocket(lua_State* L, int, const struct sockaddr_in&) throw();
    virtual ~IPSocket();
#define IPSOCKET_SUBCLASS_PROTOTYPE() \
    virtual int Lua_GetAddressParts(lua_State* L) throw(); \
    virtual int Lua_SetAddressPart(lua_State* L) throw(); \
    virtual int Lua_SetBlocking(lua_State* L) throw(); \
    virtual int Lua_GetPrintableAddress(lua_State* L) throw();
#define IPSOCKET_SUBCLASS_IMP(O) \
    int O::Lua_GetAddressParts(lua_State* L) throw() { return Lua_SubGetAddressParts(L); } \
    int O::Lua_SetAddressPart(lua_State* L) throw() { return Lua_SubSetAddressPart(L); } \
    int O::Lua_SetBlocking(lua_State* L) throw() { return Lua_SubSetBlocking(L); } \
    int O::Lua_GetPrintableAddress(lua_State* L) throw() { return Lua_SubGetPrintableAddress(L); }
    int Lua_SubGetAddressParts(lua_State* L) throw();
    int Lua_SubSetAddressPart(lua_State* L) throw();
    int Lua_SubSetBlocking(lua_State* L) throw();
    int Lua_SubGetPrintableAddress(lua_State* L) throw();
    int SetBlocking(lua_State* L, bool blocking) throw();
  protected:
    char* addrhost;
    bool addrhostset, addrportset, bound;
    in_port_t addrport;
    struct sockaddr_in addr;
    int Lua_SubApplyAddress(lua_State* L) throw();
    int sock;
  };
  class EXPORT TCPSocket : public SocketStream, IPSocket {
  public:
    TCPSocket() throw(std::bad_alloc,int);
    TCPSocket(lua_State*,int,const struct sockaddr_in&) throw();
    virtual int Lua_ApplyAddress(lua_State* L) throw();
    virtual size_t ReceiveBytes(void* buf, size_t bytes, FailReason& error) throw();
    virtual size_t SendBytes(const void* buf, size_t bytes, FailReason& error) throw();
    IPSOCKET_SUBCLASS_PROTOTYPE();
    PROTOCOL_PROTOTYPE();
  };
  class EXPORT TCPListenSocket : public ListenSocketStream, IPSocket {
  public:
    TCPListenSocket() throw(std::bad_alloc,int);
    virtual int Lua_ApplyAddress(lua_State* L) throw();
    virtual int Lua_Accept(lua_State* L) throw();
    IPSOCKET_SUBCLASS_PROTOTYPE();
    PROTOCOL_PROTOTYPE();
  };
  class EXPORT UDPSocket : public SocketDgram, IPSocket {
  public:
    UDPSocket() throw(std::bad_alloc,int);
    virtual int Lua_ApplyAddress(lua_State* L) throw();
    virtual size_t ReceiveDgram(void* buf, size_t max_size, FailReason& error) throw();
    virtual bool SendDgram(const void* buf, size_t bytes, FailReason& error) throw();
    IPSOCKET_SUBCLASS_PROTOTYPE();
    PROTOCOL_PROTOTYPE();
  };
  class UDPSlaveSocket;
#define UDP_MTU 65507
  class EXPORT UDPListenSocket : public ListenSocketDgram, IPSocket {
  public:
    UDPListenSocket(lua_State* L) throw(std::bad_alloc,int);
    virtual ~UDPListenSocket();
    virtual int Lua_ApplyAddress(lua_State* L) throw();
    virtual int Lua_Accept(lua_State* L) throw();
    IPSOCKET_SUBCLASS_PROTOTYPE();
    PROTOCOL_PROTOTYPE();
  private:
    friend class UDPSlaveSocket;
    void Massage() throw(std::bad_alloc);
    std::map<IP::addr_port_t, UDPSlaveSocket*> slaves;
    std::deque<IP::DeferredDgram> recruits;
    lua_State* L; // this is evil and could haunt me later
    int8_t buf[UDP_MTU];
  };
  class EXPORT UDPSlaveSocket : public SocketDgram {
  public:
    UDPSlaveSocket(UDPListenSocket* master, IP::addr_port_t addr) throw();
    virtual ~UDPSlaveSocket();
    virtual int Lua_ApplyAddress(lua_State* L) throw();
    virtual int Lua_GetAddressParts(lua_State* L) throw();
    virtual int Lua_SetAddressPart(lua_State* L) throw();
    virtual int Lua_SetBlocking(lua_State* L) throw();
    virtual int Lua_GetPrintableAddress(lua_State* L) throw();
    virtual size_t ReceiveDgram(void* buf, size_t max_size, FailReason& error) throw();
    virtual bool SendDgram(const void* buf, size_t bytes, FailReason& error) throw();
  private:
    friend class UDPListenSocket;
    IP::addr_port_t addr;
    std::deque<IP::DeferredMessage> packets;
    UDPListenSocket* master;
  };
}

#endif
