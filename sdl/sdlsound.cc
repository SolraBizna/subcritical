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
#include "subcritical/sound.h"

#include "sdlman.h"
#include <math.h>
#include <assert.h>

using namespace SubCritical;

static void AudioCallback(void* userdata, Uint8* stream, int len);

class EXPORT SDLSound : public SoundDevice {
 public:
  SDLSound(SoundStream* slave) throw(const char*);
  virtual ~SDLSound();
  PROTOCOL_PROTOTYPE();
  friend void AudioCallback(void* userdata, Uint8* stream, int len);
};

PROTOCOL_IMP_PLAIN(SDLSound, SoundDevice);

static void AudioCallback(void* userdata, Uint8* stream, int len) {
  size_t rem = len / 4;
  Frame* out = (Frame*)stream;
  SDLSound* fakethis = (SDLSound*)userdata;
  fakethis->slave->Mix(out, rem);
}

SDLSound::SDLSound(SoundStream* slave) throw(const char*) : SoundDevice(slave) {
  InitializeSubsystem(SDL_INIT_AUDIO);
  SDL_AudioSpec desired;
  desired.freq = slave->GetFramerate();
  desired.format = little_endian ? AUDIO_S16LSB : AUDIO_S16MSB;
  desired.channels = 2; // stereo
  desired.samples = 512; // a good sane value
  desired.callback = AudioCallback;
  desired.userdata = this;
  if(SDL_OpenAudio(&desired, NULL)) {
    QuitSubsystem(SDL_INIT_AUDIO);
    throw (const char*)SDL_GetError();
  }
  SDL_PauseAudio(0);
}

SDLSound::~SDLSound() {
  SDL_CloseAudio();
  QuitSubsystem(SDL_INIT_AUDIO);
}

SUBCRITICAL_CONSTRUCTOR(SDLSound)(lua_State* L) {
  try {
    (new SDLSound(lua_toobject(L, 1, SoundStream)))->Push(L);
    return 1;
  }
  catch(const char* e) {
    lua_pushnil(L);
    lua_pushstring(L, e);
    return 2;
  }
}
