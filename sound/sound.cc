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

#include "sound.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

using namespace SubCritical;

namespace SoundOpcode {
  enum SoundOpcode {
    Sentinel = 0,
    Nop,
    PlayMonoBuffer,
    PlayStereoBuffer,
    PlayStream,
    StopPlayback,
    ClearQueue, // eats up to this point as soon as it is detected in the queue
    Max
  };
};

static const struct ObjectMethod SBMethods[] = {
  METHOD("GetLength", &SoundBuffer::Lua_GetLength),
  NOMOREMETHODS(),
};

PROTOCOL_IMP(SoundBuffer, Object, SBMethods);
PROTOCOL_IMP_PLAIN(MonoSoundBuffer, SoundBuffer);
PROTOCOL_IMP_PLAIN(StereoSoundBuffer, SoundBuffer);

int SoundStream::Lua_GetFramerate(lua_State* L) const throw() {
  lua_pushnumber(L, GetFramerate());
  return 1;
}

static const struct ObjectMethod SSMethods[] = {
  METHOD("GetFramerate", &SoundStream::Lua_GetFramerate),
  METHOD("GetSamplerate", &SoundStream::Lua_GetFramerate),
  NOMOREMETHODS(),
};

PROTOCOL_IMP(SoundStream, Object, SSMethods);

MonoSoundBuffer::~MonoSoundBuffer() { free(buffer); }
StereoSoundBuffer::~StereoSoundBuffer() { free(buffer); }

int MonoSoundBuffer::Lua_GetLength(lua_State* L) throw() {
  lua_pushnumber(L, frames / (lua_Number)framerate);
  return 1;
}

int StereoSoundBuffer::Lua_GetLength(lua_State* L) throw() {
  lua_pushnumber(L, frames / (lua_Number)framerate);
  return 1;
}

MonoSoundBuffer::MonoSoundBuffer(uint32_t frames, uint32_t framerate) throw(std::bad_alloc)
  : frames(frames),framerate(framerate) {
  buffer = (Sample*)malloc(sizeof(Sample)*frames);
  if(!buffer) throw std::bad_alloc();
}

StereoSoundBuffer::StereoSoundBuffer(uint32_t frames, uint32_t framerate) throw(std::bad_alloc)
  : frames(frames),framerate(framerate) {
  buffer = (Frame*)malloc(sizeof(Frame)*frames);
  if(!buffer) throw std::bad_alloc();
}

struct SoundCommand {
  SoundOpcode::SoundOpcode op;
  PanMatrix pan;
  uint16_t rate; // Q8.8
  int16_t repeats; // only applies for PlayBuffer, PlayStereoBuffer
  void* target;
  uint32_t pan_present:1, rate_present:1,
    flag1:1, flag2:1, flag3:1, flag4:1,
    delay:26; // in target samples
  uint32_t loop_left, loop_right;
  lua_Number delay_error;
};

class LOCAL SubCritical::SoundChannel {
public:
  inline bool QueueCommand(const struct SoundCommand& command) {
    if(((back+1)&qmask) == front) return false;
    q[back] = command;
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
    __sync_synchronize();
#else
#warning There may be a tiny, tiny, tiny chance (roughly 200,000,000,000,000 to one) that attempting to queue a SoundCommand will crash. Add fence code here. If your compiler supports "Intel Itanium Processor-specific Application Binary Interface" section 7.4 (which can apply on all platforms, not just Itanium), just enable the code above this warning.
#endif
    back = (back + 1) & qmask;
    return true;
  }
  inline void HandleNextCommand() {
    if(front != back) {
      for(size_t n = front; n != back; n = (n + 1) & qmask) {
	if(q[n].op == SoundOpcode::ClearQueue) {
	  front = (n + 1) & qmask;
	  delay = -1;
          // keep searching, there may be another ClearQueue
	}
      }
      if(front == back) return;
      const SoundCommand& Q = q[front];
      if(delay < 0 && Q.delay) {
	delay = Q.delay;
	delay_error += Q.delay_error;
	while(delay_error >= 1) {
	  ++delay;
	  --delay_error;
	}
      }
      else if(delay <= 0) {
	delay = -1;
	bool have_rate = false;
	uint16_t target_rate = 256;
	switch(Q.op) {
	default:
	  break;
	case SoundOpcode::PlayMonoBuffer:
	case SoundOpcode::PlayStereoBuffer:
	  // these have no meaning for PlayStream
	  repeats = Q.repeats;
	  target_position = 0;
	  loop_left = Q.loop_left;
	  loop_right = Q.loop_right;
	  if(!loop_right) {
	    uint32_t frames;
	    if(Q.op == SoundOpcode::PlayMonoBuffer) frames = ((MonoSoundBuffer*)Q.target)->frames;
	    else frames = ((StereoSoundBuffer*)Q.target)->frames;
	    loop_right = frames;
	  }
	case SoundOpcode::PlayStream:
	  irp = 32768; // 100% new sample
	  have_rate = true;
	  target_type = Q.op;
	  target = Q.target;
	  break;
	case SoundOpcode::StopPlayback:
	  target_type = SoundOpcode::Nop;
	  break;
	}
	if(Q.pan_present) {
	  pan[0] = Q.pan[0];
	  pan[1] = Q.pan[1];
	  pan[2] = Q.pan[2];
	  pan[3] = Q.pan[3];
	}
	if(Q.rate_present) {
	  have_rate = true;
	  target_rate = Q.rate;
	}
	if(have_rate) {
	  uint32_t in_rate;
	  switch(target_type) {
	  case SoundOpcode::PlayMonoBuffer: in_rate = ((MonoSoundBuffer*)target)->framerate; break;
	  case SoundOpcode::PlayStereoBuffer: in_rate = ((StereoSoundBuffer*)target)->framerate; break;
	  case SoundOpcode::PlayStream: in_rate = ((SoundStream*)target)->GetFramerate(); break;
	  default: in_rate = out_rate; break;
	  }
	  rate = (uint32_t)((uint64_t)target_rate * 128 * in_rate / out_rate);
	}
	if(Q.flag1) flags[0] = true;
	if(Q.flag2) flags[1] = true;
	if(Q.flag3) flags[2] = true;
	if(Q.flag4) flags[3] = true;
	front = (front + 1) & qmask;
	HandleNextCommand();
      }
    }
  }
  inline void MixOut(Frame* buffer, Frame* aux, size_t frames) {
    HandleNextCommand();
    size_t frames_left = frames;
    if(delay >= 0) {
      if((size_t)delay < frames_left) frames_left = delay;
    }
    if(target_type == SoundOpcode::Nop) {
      if(delay >= 0)
        delay -= frames_left;
      if(delay == 0) {
	frames -= frames_left;
	buffer += frames_left;
	HandleNextCommand();
	MixOut(buffer, aux, frames);
      }
      return;
    }
    if(rate == 32768 && !(irp&32767)) { // 1:1, not between samples
      switch(target_type) {
      default: break; // NOTREACHED
      case SoundOpcode::PlayStream:
	memset(aux, 0, frames_left*sizeof(Frame));
	((SoundStream*)target)->Mix(aux, frames_left);
	for(Frame* p = aux;
	    frames_left > 0; --frames_left, (delay>=0?--delay:0), --frames, ++p, ++buffer) {
	  (*buffer)[0] += (*p)[0] * pan[0] / 4096 + (*p)[1] * pan[1] / 4096;
	  (*buffer)[1] += (*p)[0] * pan[2] / 4096 + (*p)[1] * pan[3] / 4096;
	}
	break;
      case SoundOpcode::PlayStereoBuffer:
	{
	  StereoSoundBuffer* target = ((StereoSoundBuffer*)this->target);
	  for(Frame* p = target->buffer + target_position;
	      frames_left > 0 && target_position < loop_right;
	      --frames_left, --frames, (delay>=0?--delay:0), ++p, ++buffer, ++target_position) {
	    (*buffer)[0] += (*p)[0] * pan[0] / 4096 + (*p)[1] * pan[1] / 4096;
	    (*buffer)[1] += (*p)[0] * pan[2] / 4096 + (*p)[1] * pan[3] / 4096;
	  }
	}
      case SoundOpcode::PlayMonoBuffer:
	{
	  MonoSoundBuffer* target = ((MonoSoundBuffer*)this->target);
	  for(Sample* p = target->buffer + target_position;
	      frames_left > 0 && target_position < loop_right;
	      --frames_left, --frames, (delay>=0?--delay:0), ++p, ++buffer, ++target_position) {
	    (*buffer)[0] += *p * pan[0] / 4096 + *p * pan[1] / 4096;
	    (*buffer)[1] += *p * pan[2] / 4096 + *p * pan[3] / 4096;
	  }
	}
	break;
      }
    }
    else {
      while(frames_left > 0) {
	if(irp >= 32768) {
	  Sample backup_frame[2];
	  backup_frame[0] = irp_frame[0];
	  backup_frame[1] = irp_frame[1];
	  irp_frame[0] = tsugi_frame[0];
	  irp_frame[1] = tsugi_frame[1];
	  switch(target_type) {
	  default: return; // NOTREACHED
	  case SoundOpcode::PlayStream:
	    ((SoundStream*)target)->Mix(&tsugi_frame, 1); // YUCK
	    break;
	  case SoundOpcode::PlayStereoBuffer:
	    {
	      StereoSoundBuffer* target = ((StereoSoundBuffer*)this->target);
	      if(target_position >= loop_right) goto bloh;
	      tsugi_frame[0] = target->buffer[target_position][0];
	      tsugi_frame[1] = target->buffer[target_position][1];
	      ++target_position;
	    }
	    break;
	  case SoundOpcode::PlayMonoBuffer:
	    {
	      MonoSoundBuffer* target = ((MonoSoundBuffer*)this->target);
	      if(target_position >= loop_right) goto bloh;
	      tsugi_frame[1] = tsugi_frame[0] = target->buffer[target_position];
	      ++target_position;
	    }
	    break;
	  }
	  irp -= 32768;
	  continue;
	bloh:
	  irp_frame[0] = backup_frame[0];
	  irp_frame[1] = backup_frame[1];
	  goto blah;
	}
	Frame inner_frame;
	do {
	  inner_frame[0] = (irp_frame[0] * (32768-irp) + tsugi_frame[0] * irp) / 32768;
	  inner_frame[1] = (irp_frame[1] * (32768-irp) + tsugi_frame[1] * irp) / 32768;
	  (*buffer)[0] += inner_frame[0] * pan[0] / 4096 + inner_frame[1] * pan[1] / 4096;
	  (*buffer)[1] += inner_frame[0] * pan[2] / 4096 + inner_frame[1] * pan[3] / 4096;
	  irp += rate;
	  --frames_left;
          if(delay >= 0) --delay;
	  --frames;
	  ++buffer;
	} while(irp < 32768 && frames_left);
      }
    }
  blah:
    if(frames_left > 0) {
      // Sound ended, delay (if any) not up
      if(repeats != 0) {
	if(repeats > 0) { --repeats; }
	switch(target_type) {
	default:
	  // NOTREACHED
	  target_position = 0;
	  break;
	case SoundOpcode::PlayMonoBuffer:
	case SoundOpcode::PlayStereoBuffer:
	  target_position -= loop_right - loop_left;
	  break;
	}
      }
      else target_type = SoundOpcode::Nop;
      MixOut(buffer, aux, frames);
    }
    else if(delay == 0) {
      HandleNextCommand();
      MixOut(buffer, aux, frames);
    }
  }
  SoundChannel(size_t qlen, uint32_t out_rate) throw(std::bad_alloc) : q((SoundCommand*)malloc(sizeof(SoundCommand)*qlen)),qlen(qlen),front(0),back(0),qmask(qlen-1),delay(-1),delay_error(0),out_rate(out_rate),target(NULL),target_type(SoundOpcode::Nop) {
    for(int n = 0; n < NUM_CHANNEL_FLAGS; ++n) flags[n] = false;
    pan[0] = pan[3] = 4096;
    pan[1] = pan[2] = 0;
  }
  ~SoundChannel() { free((void*)q); }
  SoundCommand* q;
  size_t qlen, front, back, qmask;
  PanMatrix pan;
  Frame irp_frame, tsugi_frame;
  uint32_t irp; // Q17.15
  int32_t delay;
  lua_Number delay_error;
  uint32_t rate; // Q17.15
  uint32_t out_rate;
  void* target;
  size_t target_position; // for Buffers only
  uint32_t loop_left, loop_right;
  SoundOpcode::SoundOpcode target_type;
  int16_t repeats;
  uint8_t flags[NUM_CHANNEL_FLAGS];
};

SoundMixer::SoundMixer(size_t num_channels, size_t qlen, size_t rate)
  throw(std::bad_alloc) : num_channels(num_channels), rate(rate) {
  channels = (SoundChannel*)calloc(sizeof(SoundChannel), num_channels);
  if(!channels) throw std::bad_alloc();
  for(size_t n = 0; n < num_channels; ++n) {
    try {
      new((void*)(channels+n)) SoundChannel(qlen, rate);
    }
    catch(...) {
      // deliberately NOT size_t, since it isn't supposed to be signed
      for(int m = n - 1; m >= 0; --m) {
	channels[m].~SoundChannel();
      }
      throw;
    }
  }
}

SoundMixer::~SoundMixer() {
  for(size_t n = 0; n < num_channels; ++n) {
    channels[n].~SoundChannel();
  }
  free(channels);
}

uint32_t SoundMixer::GetFramerate() const throw() {
  return rate;
}

int SoundMixer::GetNumChannels() const throw() {
  return num_channels;
}

int SoundMixer::Lua_GetNumChannels(lua_State* L) throw() {
  lua_pushnumber(L, num_channels);
  return 1;
}

void SoundMixer::Mix(Frame* buffer, size_t count) throw() {
  Frame aux[count];
  memset(buffer, count * sizeof(Frame), 0);
  for(size_t ch = 0; ch < num_channels; ++ch) {
    channels[ch].MixOut(buffer, aux, count);
  }
}

static Pan ToPan(lua_State* L, int i) {
  return (Pan)floor(lua_tonumber(L,i)*4096);
}

static int ParseSoundCommand(lua_State* L, int i, SoundCommand& cmd, bool target, uint32_t out_rate) {
  cmd.op = SoundOpcode::Nop;
  cmd.repeats = 0;
  cmd.pan_present = 0;
  cmd.rate_present = 0;
  cmd.flag4 = cmd.flag3 = cmd.flag2 = cmd.flag1 = 0;
  cmd.delay = 0;
  cmd.delay_error = 0;
  cmd.loop_left = 0;
  cmd.loop_right = 0;
  if(!lua_istable(L, i)) {
    if(lua_isnil(L, i)) return 0;
    else return luaL_typerror(L, i, "table");
  }
  if(target) {
    cmd.target = NULL; // eek!
    lua_getfield(L, i, "sound");
    if(lua_isnil(L, -1))
      lua_pop(L, 1);
    else {
      Object* o = lua_toobject(L, -1, Object);
      lua_pop(L, 1);
      if(o->IsA("SoundStream"))
	cmd.op = SoundOpcode::PlayStream;
      else if(o->IsA("MonoSoundBuffer"))
	cmd.op = SoundOpcode::PlayMonoBuffer;
      else if(o->IsA("StereoSoundBuffer"))
	cmd.op = SoundOpcode::PlayStereoBuffer;
      else return luaL_error(L, "\"sound\" parameter must be either nil or a SoundStream, MonoSoundBuffer, or StereoSoundBuffer");
      cmd.target = (void*)o;
      if(o->IsA("SoundBuffer")) {
	lua_Integer len;
	uint32_t framerate;
	if(o->IsA("MonoSoundBuffer")) {
	  len = ((MonoSoundBuffer*)o)->frames;
	  framerate = ((MonoSoundBuffer*)o)->framerate;
	}
	// we should really assert here
	else {
	  len = ((StereoSoundBuffer*)o)->frames;
	  framerate = ((StereoSoundBuffer*)o)->framerate;
	}
	lua_getfield(L, i, "loop_left");
	if(!lua_isnil(L, -1)) {
	  if(!lua_isnumber(L, -1)) return luaL_error(L, "\"loop_left\" parameter must be a number");
	  lua_Integer ll = (lua_Integer)(lua_tonumber(L, -1) * framerate);
	  if(ll < 0) return luaL_error(L, "loop_left must be >= 0");
	  else if(ll >= len) return luaL_error(L, "loop_left must be inside the sample");
	  cmd.loop_left = ll;
	}
	lua_pop(L, 1);
	lua_getfield(L, i, "loop_right");
	if(!lua_isnil(L, -1)) {
	  if(!lua_isnumber(L, -1)) return luaL_error(L, "\"loop_right\" parameter must be a number");
	  lua_Integer lr = (lua_Integer)(lua_tonumber(L, -1) * framerate);
	  if(lr <= 0) return luaL_error(L, "loop_right must be > 0");
	  else if(lr > len) return luaL_error(L, "loop_right must be inside the sample");
	  else if(lr <= (lua_Integer)cmd.loop_left) return luaL_error(L, "loop_right must be AFTER loop_left");
	  cmd.loop_right = lr;
	}
	lua_pop(L, 1);
      }
    }
    lua_getfield(L, i, "repeats");
    if(!lua_isnil(L, -1)) {
      if(lua_isboolean(L, -1)) {
	cmd.repeats = -lua_toboolean(L, -1);
      }
      else {
	if(!lua_isnumber(L, -1)) return luaL_error(L, "\"repeats\" parameter must be a number");
	lua_Integer repeats = lua_tointeger(L, -1);
	cmd.repeats = repeats;
      }
      lua_pop(L, 1);
    }
    else {
      lua_pop(L, 1);
      cmd.repeats = 0;
    }
  }
  lua_getfield(L, i, "pan");
  if(!lua_isnil(L, -1)) {
    if(!lua_istable(L, -1)) return luaL_error(L, "\"pan\" parameter must be a table with 1, 2, or 4 members");
    cmd.pan_present = 1;
    lua_Integer num = lua_rawlen(L, -1);
    switch(num) {
    case 1:
      lua_rawgeti(L, -1, 1);
      if(!lua_isnumber(L, -1)) return luaL_error(L, "\"pan\" matrices must contain numbers");
      cmd.pan[2] = cmd.pan[1] = 0;
      cmd.pan[3] = cmd.pan[0] = ToPan(L, -1);
      lua_pop(L, 2);
      break;
    case 2:
      lua_rawgeti(L, -1, 1);
      lua_rawgeti(L, -2, 2);
      if(!lua_isnumber(L, -1) || !lua_isnumber(L, -2)) return luaL_error(L, "\"pan\" matrices must contain numbers");
      cmd.pan[2] = cmd.pan[1] = 0;
      cmd.pan[0] = ToPan(L, -2);
      cmd.pan[3] = ToPan(L, -1);
      lua_pop(L, 3);
      break;
    case 4:
      lua_rawgeti(L, -1, 1);
      lua_rawgeti(L, -2, 2);
      lua_rawgeti(L, -3, 3);
      lua_rawgeti(L, -4, 4);
      if(!lua_isnumber(L, -1) || !lua_isnumber(L, -2) || !lua_isnumber(L, -3) || !lua_isnumber(L, -4)) return luaL_error(L, "\"pan\" matrices must contain numbers");
      cmd.pan[0] = ToPan(L, -4);
      cmd.pan[1] = ToPan(L, -3);
      cmd.pan[2] = ToPan(L, -2);
      cmd.pan[3] = ToPan(L, -1);
      lua_pop(L, 5);
      break;
    }
  }
  else lua_pop(L, 1);
  lua_getfield(L, i, "rate");
  if(!lua_isnil(L, -1)) {
    if(!lua_isnumber(L, -1)) return luaL_error(L, "\"rate\" parameter must be a number");
    lua_Number rate = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if(rate < 0 || rate >= 256.0) return luaL_error(L, "rate out of range (0 <= rate < 256)");
    cmd.rate_present = 1;
    cmd.rate = (uint16_t)floor(rate*256);
  }
  else lua_pop(L, 1);
  lua_getfield(L, i, "delay");
  if(!lua_isnil(L, -1)) {
    if(!lua_isnumber(L, -1)) return luaL_error(L, "\"delay\" parameter must be a number");
    lua_Number delay = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if(delay < 0) return luaL_error(L, "negative \"delay\" specified");
    delay = delay * out_rate;
    cmd.delay = (uint32_t)floor(delay);
    cmd.delay_error = delay - cmd.delay;
  }
  else lua_pop(L, 1);
  lua_getfield(L, i, "flag1");
  if(lua_toboolean(L, -1)) cmd.flag1 = 1;
  lua_pop(L, 1);
  lua_getfield(L, i, "flag2");
  if(lua_toboolean(L, -1)) cmd.flag2 = 1;
  lua_pop(L, 1);
  lua_getfield(L, i, "flag3");
  if(lua_toboolean(L, -1)) cmd.flag3 = 1;
  lua_pop(L, 1);
  lua_getfield(L, i, "flag4");
  if(lua_toboolean(L, -1)) cmd.flag4 = 1;
  lua_pop(L, 1);
  return 0;
}

int SoundMixer::Lua_Play(lua_State* L) {
  lua_Integer channel = luaL_checkinteger(L, 1) - 1;
  if(channel < 0 || (size_t)channel >= num_channels) return luaL_error(L, "channel %d out of range", channel) + 1;
  SoundCommand cmd;
  ParseSoundCommand(L, 2, cmd, true, rate);
  if(channels[channel].QueueCommand(cmd)) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "command queue full");
    return 2;
  }
}

int SoundMixer::Lua_Stop(lua_State* L) {
  lua_Integer channel = luaL_checkinteger(L, 1) - 1;
  if(channel < 0 || (size_t)channel >= num_channels) return luaL_error(L, "channel %d out of range", channel + 1);
  SoundCommand cmd;
  ParseSoundCommand(L, 2, cmd, false, rate);
  cmd.op = SoundOpcode::StopPlayback;
  if(channels[channel].QueueCommand(cmd)) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "command queue full");
    return 2;
  }
}

int SoundMixer::Lua_ClearQueue(lua_State* L) {
  lua_Integer channel = luaL_checkinteger(L, 1) - 1;
  if(channel < 0 || (size_t)channel >= num_channels) return luaL_error(L, "channel %d out of range", channel + 1);
  SoundCommand cmd;
  cmd.op = SoundOpcode::ClearQueue;
  if(channels[channel].QueueCommand(cmd)) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "command queue full (rofl)");
    return 2;
  }
}

int SoundMixer::Lua_TestFlag(lua_State* L) {
  lua_Integer channel = luaL_checkinteger(L, 1) - 1;
  if(channel < 0 || (size_t)channel >= num_channels) return luaL_error(L, "channel %d out of range", channel + 1);
  int flag = luaL_checkinteger(L, 2) - 1;
  if(flag < 0 || flag >= NUM_CHANNEL_FLAGS) return luaL_error(L, "flag %d out of range", flag + 1);
  uint8_t set = !!channels[channel].flags[flag];
  channels[channel].flags[flag] = 0;
  lua_pushboolean(L, set);
  return 1;
}

SUBCRITICAL_CONSTRUCTOR(SoundMixer)(lua_State* L) {
  lua_Integer channels = luaL_checkinteger(L, 1);
  lua_Integer qlen = luaL_checkinteger(L, 2);
  lua_Integer samplerate = luaL_checkinteger(L, 3);
  if(channels <= 0 || (channels&(channels-1))) return luaL_error(L, "channels must be a positive, >0 power of 2");
  if(qlen <= 0 || (qlen&(qlen-1))) return luaL_error(L, "queue length must be a positive, >0 power of 2");
  if(samplerate <= 0) return luaL_error(L, "sample rate must be a positive integer");
  (new SoundMixer(channels, qlen, samplerate))->Push(L);
  return 1;
}

static const struct ObjectMethod SMMethods[] = {
  METHOD("GetNumChannels", &SoundMixer::Lua_GetNumChannels),
  METHOD("Play", &SoundMixer::Lua_Play),
  METHOD("Stop", &SoundMixer::Lua_Stop),
  METHOD("ClearQueue", &SoundMixer::Lua_ClearQueue),
  METHOD("TestFlag", &SoundMixer::Lua_TestFlag),
  NOMOREMETHODS(),
};

PROTOCOL_IMP(SoundMixer, SoundStream, SMMethods);

PROTOCOL_IMP_PLAIN(SoundMaster, Object);
PROTOCOL_IMP_PLAIN(SoundDevice, SoundMaster);

SoundMaster::SoundMaster(SoundStream* slave) : slave(slave) {}
SoundDevice::SoundDevice(SoundStream* slave) : SoundMaster(slave) {}
