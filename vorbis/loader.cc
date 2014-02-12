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

using namespace SubCritical;

class LOCAL VorbisLoader : public SoundLoader {
 public:
  PROTOCOL_PROTOTYPE();
  virtual SoundBuffer* Load(const char* path) throw();
};

SoundBuffer* VorbisLoader::Load(const char* path) throw() {
  OggVorbis_File ogg;
  // ov_open can apparently cause major headaches on Windows
  if(ov_fopen(const_cast<char*>(path), &ogg)) return NULL;
  vorbis_info* info = ov_info(&ogg, -1);
  if(info->channels != 1 && info->channels != 2) {
    fprintf(stderr, "Only 1- and 2-channel Ogg Vorbis bitstreams are supported.\n");
    return NULL;
  }
  uint32_t rate = info->rate;
  long frames = ov_pcm_total(&ogg, -1);
  if(frames == OV_EINVAL) {
    ov_clear(&ogg);
    fprintf(stderr, "Unseekable Ogg?\n");
    return NULL;
  }
  SoundBuffer* ret;
  char* dest;
  uint32_t rem;
  if(info->channels == 2) {
    StereoSoundBuffer* stereo = new StereoSoundBuffer(frames, rate);
    ret = stereo;
    dest = (char*)stereo->buffer;
    rem = frames * sizeof(Frame);
  }
  else {
    MonoSoundBuffer* mono = new MonoSoundBuffer(frames, rate);
    ret = mono;
    dest = (char*)mono->buffer;
    rem = frames * sizeof(Sample);
  }
  int stream = -1;
  while(rem > 0) {
    long read = ov_read(&ogg, dest, rem, !little_endian, 2, 1, &stream);
    if(read <= 0) {
      ov_clear(&ogg);
      fprintf(stderr, "Ogg not okay! Fail! (Is this a multi-bitstream Ogg, perchance? %i)\n", (int)read);
      delete ret;
      return NULL;
    }
    rem -= read;
    dest += read;
  }
  ov_clear(&ogg);
  return ret;
}

SUBCRITICAL_CONSTRUCTOR(VorbisLoader)(lua_State* L) {
  (new VorbisLoader())->Push(L);
  return 1;
}

PROTOCOL_IMP_PLAIN(VorbisLoader, SoundLoader);
