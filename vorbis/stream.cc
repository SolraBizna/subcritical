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

#include "subcritical/sound.h"

#include <vorbis/vorbisfile.h>
#include <vorbis/codec.h>

#include <string.h>
#include <stdlib.h>

using namespace SubCritical;

class LOCAL VorbisStream : public SoundStream {
 public:
  PROTOCOL_PROTOTYPE();
  VorbisStream(const char* path);
  virtual ~VorbisStream();
  virtual void Mix(Frame* out, size_t count) throw();
  virtual uint32_t GetFramerate() const throw();
  virtual int Lua_GetTags(lua_State* L) const throw();
 private:
  uint32_t framerate;
  uint64_t loop_left, loop_right, pos;
  int channels;
  bool failed;
  OggVorbis_File ogg;
  vorbis_comment* comm;
  int stream;
};

static const struct ObjectMethod methods[] = {
  METHOD("GetTags", &VorbisStream::Lua_GetTags),
  NOMOREMETHODS(),
};

PROTOCOL_IMP(VorbisStream, SoundStream, methods);

int VorbisStream::Lua_GetTags(lua_State* L) const throw() {
  lua_createtable(L, comm->comments, 0);
  for(int n = 0; n < comm->comments; ++n) {
    char* c = comm->user_comments[n];
    lua_pushstring(L, c);
    lua_rawseti(L, -2, n+1);
  }
  return 1;
}

uint32_t VorbisStream::GetFramerate() const throw() {
  return framerate;
}

VorbisStream::~VorbisStream() {
  ov_clear(&ogg); 
}

static void DoubleUp(Sample* p, size_t mono_count) {
  for(int32_t n = mono_count-1; n >= 0; --n) {
    p[n*2+1] = p[n*2] = p[n];
  }
}

void VorbisStream::Mix(Frame* out, size_t count) throw() {
  if(failed) return;
  if(pos == loop_right) {
    pos = loop_left;
    if(ov_pcm_seek_lap(&ogg, loop_left)) {
      fprintf(stderr, "Seek-lap failed. Non-seekable Ogg? Faulty LOOP_START tag?");
      failed = true;
      return;
    }
  }
  size_t read_count = count;
  if(read_count + pos > loop_right)
    read_count = loop_right - pos;
  read_count *= sizeof(Sample) * channels;
  long read = ov_read(&ogg, (char*)out, read_count, !little_endian, 2, 1, &stream);
  if(read < 0) {
    fprintf(stderr, "Ogg not okay! Fail! (Is this a multi-bitstream Ogg, perchance? %i)\n", (int)read);
    failed = true;
    return;
  }
  else if(read == 0) {
    // DON'T set loop_right here, the ogg could be growing!
    pos = loop_left;
    if(ov_pcm_seek_lap(&ogg, loop_left)) {
      fprintf(stderr, "Seek-lap failed. Non-seekable Ogg? Faulty LOOP_START tag?");
      failed = true;
      Mix(out, count);
      return;
    }
  }
  uint32_t samples_read = read/sizeof(Sample)/channels;
  if(channels == 1)
    DoubleUp((Sample*)out, samples_read);
  out += samples_read;
  count -= samples_read;
  pos += samples_read;
  if(count > 0) Mix(out, count);
}

VorbisStream::VorbisStream(const char* path)
  : loop_left(0), loop_right(~(uint64_t)0), pos(0), failed(false), stream(0) {
  if(ov_fopen(const_cast<char*>(path), &ogg)) throw (const char*)"Unable to open";
  vorbis_info* info = ov_info(&ogg, -1);
  if(info->channels != 1 && info->channels != 2)
    throw (const char*)"Only 1- and 2-channel Ogg Vorbis bitstreams are supported.\n";
  channels = info->channels;
  framerate = info->rate;
  comm = ov_comment(&ogg, -1);
  pos = loop_left = (uint64_t)(ov_time_tell(&ogg) * framerate);
  for(int n = 0; n < comm->comments; ++n) {
    char* c = comm->user_comments[n];
    if(!strncasecmp(c, "LOOP_START=", 11)) {
      double t;
      char* p;
      t = strtod(c + 11, &p);
      if(p && *p) {
	fprintf(stderr, "WARNING: Invalid LOOP_START tag\n");
	continue;
      }
      loop_left = (uint64_t)(t * framerate);
    }
    else if(!strncasecmp(c, "LOOP_END=", 9)) {
      double t;
      char* p;
      t = strtod(c + 9, &p);
      if(p && *p) {
	fprintf(stderr, "WARNING: Invalid LOOP_END tag\n");
	continue;
      }
      loop_right = (uint64_t)(t * framerate);
    }
  }
}

SUBCRITICAL_CONSTRUCTOR(VorbisStream)(lua_State* L) {
  try {
    (new VorbisStream(GetPath(L,1)))->Push(L);
    return 1;
  }
  catch(const char* e) {
    lua_pushnil(L);
    lua_pushstring(L, e);
    return 2;
  }
}
