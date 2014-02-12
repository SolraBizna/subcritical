// -*- c++ -*-
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
#ifndef _SUBCRITICAL_SOCKET_H
#define _SUBCRITICAL_SOCKET_H

#include "subcritical/core.h"

#include <new>

namespace SubCritical {
  class EXPORT Socket : public Object {
  public:
    virtual int Lua_GetAddressParts(lua_State* L) throw() = 0;
    virtual int Lua_SetAddressPart(lua_State* L) throw() = 0;
    virtual int Lua_ApplyAddress(lua_State* L) throw() = 0;
    virtual int Lua_SetBlocking(lua_State* L) throw() = 0;
    virtual int Lua_GetPrintableAddress(lua_State* L) throw() = 0;
    PROTOCOL_PROTOTYPE();
    enum FailReason { Broken=0, NotReady=1, PacketTooBig=2, NumFailReasons };
    static const char* FailReasonToString(FailReason reason) throw();
  };
  class EXPORT ListenSocket : public Socket {
  public:
    virtual int Lua_Accept(lua_State* L) throw() = 0;
    PROTOCOL_PROTOTYPE();
  };
  // these three only exist for Identity()'s/IsA()'s sake
  class EXPORT ListenSocketDgram : public ListenSocket { public: PROTOCOL_PROTOTYPE(); };
  class EXPORT ListenSocketStream : public ListenSocket { public: PROTOCOL_PROTOTYPE(); };
  class EXPORT ConnectedSocket : public Socket {
  public:
    ConnectedSocket() throw();
    ~ConnectedSocket();
    PROTOCOL_PROTOTYPE();
    virtual int Lua_Receive(lua_State* L) throw() = 0;
    virtual int Lua_Send(lua_State* L) throw() = 0;
  private:
    void* buf;
    size_t bufsz;
  protected:
    void* CheckBuf(size_t size) throw(std::bad_alloc);
    void* GetBuf(size_t& size) throw();
  };
  class EXPORT SocketDgram : public ConnectedSocket {
  public:
    virtual size_t ReceiveDgram(void* buf, size_t max_size, FailReason& error) throw() = 0;
    virtual int Lua_Receive(lua_State* L) throw();
    virtual bool SendDgram(const void* buf, size_t bytes, FailReason& error) throw() = 0;
    virtual int Lua_Send(lua_State* L) throw();
    PROTOCOL_PROTOTYPE();
  };
  class EXPORT SocketStream : public ConnectedSocket {
  public:
    virtual size_t ReceiveBytes(void* buf, size_t bytes, FailReason& error) throw() = 0;
    virtual int Lua_Receive(lua_State* L) throw();
    virtual size_t SendBytes(const void* buf, size_t bytes, FailReason& error) throw() = 0;
    virtual int Lua_Send(lua_State* L) throw();
    PROTOCOL_PROTOTYPE();
  };
}

#endif
