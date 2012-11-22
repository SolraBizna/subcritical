// -*- c++ -*-
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
#ifndef _SUBCRITICAL_SOUND_H
#define _SUBCRITICAL_SOUND_H

#include "subcritical/core.h"

#include <new> // for bad_alloc

namespace SubCritical {
#define NUM_CHANNEL_FLAGS 4
  typedef int16_t Sample, Frame[2];
  typedef int16_t Pan, PanMatrix[4]; // Q3.12
  class EXPORT SoundStream : public Object {
  public:
    PROTOCOL_PROTOTYPE();
    virtual uint32_t GetFramerate() const throw() = 0;
    int Lua_GetFramerate(lua_State* L) const throw();
    // count is a frame count
    // For a given SoundStream, Mix should be called either only in the sound
    // thread or only in the main thread.
    virtual void Mix(Frame* buffer, size_t count) throw() = 0;
  };
  class EXPORT SoundBuffer : public Object {
  public:
    PROTOCOL_PROTOTYPE();
    virtual int Lua_GetLength(lua_State* L) throw() = 0;
  };
  class EXPORT MonoSoundBuffer : public SoundBuffer {
  public:
    PROTOCOL_PROTOTYPE();
    MonoSoundBuffer(uint32_t frames, uint32_t framerate) throw(std::bad_alloc);
    virtual ~MonoSoundBuffer();
    virtual int Lua_GetLength(lua_State* L) throw();
    Sample* buffer;
    uint32_t frames;
    uint32_t framerate;
  };
  class EXPORT StereoSoundBuffer : public SoundBuffer {
  public:
    PROTOCOL_PROTOTYPE();
    StereoSoundBuffer(uint32_t frames, uint32_t framerate) throw(std::bad_alloc);
    virtual ~StereoSoundBuffer();
    virtual int Lua_GetLength(lua_State* L) throw();
    Frame* buffer;
    uint32_t frames;
    uint32_t framerate;
  };
  class SoundChannel;
  class EXPORT SoundMixer : public SoundStream {
  public:
    PROTOCOL_PROTOTYPE();
    SoundMixer(size_t channels, size_t qlen, size_t samplerate) throw(std::bad_alloc);
    virtual ~SoundMixer();
    virtual uint32_t GetFramerate() const throw();
    int GetNumChannels() const throw();
    int Lua_GetNumChannels(lua_State* L) throw();
    virtual void Mix(Frame* buffer, size_t count) throw();
    // Play(channel, {sound=..., pan=..., rate=..., repeats=..., delay=..., flag#=...})
    int Lua_Play(lua_State* L); // first four commands
    // Stop(channel, {pan=..., rate=..., delay=..., flag#=...})
    int Lua_Stop(lua_State* L);
    int Lua_ClearQueue(lua_State* L);
    int Lua_TestFlag(lua_State* L); // destructive test
    void BeginTransaction() throw();
    int Lua_BeginTransaction(lua_State* L);
    void CommitTransaction() throw();
    int Lua_CommitTransaction(lua_State* L);
    void RollbackTransaction() throw();
    int Lua_RollbackTransaction(lua_State* L);
  private:
    SoundChannel* channels;
    size_t num_channels;
    uint32_t rate;
    /* ONLY locked during MixOut, CommitTransaction, and RollbackTransaction */
    Mutex mutex;
  };
  class EXPORT SoundMaster : public Object {
  public:
    PROTOCOL_PROTOTYPE();
  protected:
    SoundMaster(SoundStream* slave);
    SoundStream* slave;
  };
  // SoundMasters other than SoundDevices need manual class-specific pumping.
  class EXPORT SoundDevice : public SoundMaster {
  public:
    PROTOCOL_PROTOTYPE();
  protected:
    SoundDevice(SoundStream* slave);
  };
  class EXPORT SoundLoader : public Object {
  public:
    PROTOCOL_PROTOTYPE();
    virtual SoundBuffer* Load(const char* file) throw() = 0;
    virtual int Lua_Load(lua_State* L) throw();
  };
};

#endif
