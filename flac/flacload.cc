/*
  This source file is part of the SubCritical core package set.
  Copyright (C) 2008-2013 Solra Bizna.

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

#include <new> // for bad_alloc
#include <setjmp.h>

using namespace SubCritical;

#include "FLAC/stream_decoder.h"

static FLAC__StreamDecoderWriteStatus callback_write(const FLAC__StreamDecoder* flac, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
static void callback_metadata(const FLAC__StreamDecoder* flac, const FLAC__StreamMetadata* metadata, void* client_data);
static void callback_fail(const FLAC__StreamDecoder* flac, FLAC__StreamDecoderErrorStatus status, void* client_data);

class FLACLoader : public SoundLoader {
public:
  PROTOCOL_PROTOTYPE();
  virtual ~FLACLoader();
  FLACLoader();
  virtual SoundBuffer* Load(const char* path) throw();
private:
  FLAC__StreamDecoder* flac;
  // transients, ick:
  friend FLAC__StreamDecoderWriteStatus callback_write(const FLAC__StreamDecoder* flac, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
  friend void callback_metadata(const FLAC__StreamDecoder* flac, const FLAC__StreamMetadata* metadata, void* client_data);
  friend void callback_fail(const FLAC__StreamDecoder* flac, FLAC__StreamDecoderErrorStatus status, void* client_data);
  SoundBuffer* ret;
  Sample* pos;
  int bits_per_sample, channels;
  jmp_buf failbuf;
};

FLACLoader::FLACLoader() {
  flac = FLAC__stream_decoder_new();
  if(!flac) throw std::bad_alloc();
}

FLACLoader::~FLACLoader() {
  FLAC__stream_decoder_delete(flac);
}

static void Mixup(const int8_t* src, Sample* dest, size_t rem) {
  UNROLL_MORE(rem,
	      *dest++ = (Sample)(((uint8_t)*src << 8) | ((uint8_t)*src ^ 0x80));
	      ++src;);
}

static FLAC__StreamDecoderWriteStatus callback_write(const FLAC__StreamDecoder* flac, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data) {
  FLACLoader* fakethis = (FLACLoader*)client_data;
  size_t rem = frame->header.blocksize;
  size_t i = 0;
  if(fakethis->bits_per_sample == 16) {
    if(fakethis->channels == 2)
      UNROLL_MORE(rem,
		  *fakethis->pos++ = buffer[0][i];
		  *fakethis->pos++ = buffer[1][i];
		  ++i;);
    else /* {fakethis->channels == 1} */
      UNROLL_MORE(rem,
		  *fakethis->pos++ = buffer[0][i++];);
  }
  else /* {fakethis->bits_per_sample == 8} */ {
    size_t count = frame->header.blocksize*fakethis->channels;
    int8_t aux[count], *p;
    p = aux;
    if(fakethis->channels == 2)
      UNROLL_MORE(rem,
		  *p++ = buffer[0][i];
		  *p++ = buffer[1][i];
		  ++i;);
    else /* {fakethis->channels == 1} */
      UNROLL_MORE(rem,
		  *p++ = buffer[0][i++];);
    Mixup(aux, fakethis->pos, count);
    fakethis->pos += count;
  }
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void callback_metadata(const FLAC__StreamDecoder* flac, const FLAC__StreamMetadata* metadata, void* client_data) {
  FLACLoader* fakethis = (FLACLoader*)client_data;
  if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    const FLAC__StreamMetadata_StreamInfo& streaminfo = metadata->data.stream_info;
    if(streaminfo.channels != 1 && streaminfo.channels != 2) {
      fprintf(stderr, "Only 1- and 2-channel FLACs are supported.\n");
      longjmp(fakethis->failbuf, 1);
    }
    if(streaminfo.bits_per_sample != 8 && streaminfo.bits_per_sample != 16) {
      fprintf(stderr, "Only 8- and 16-bit FLACs are supported.\n");
      longjmp(fakethis->failbuf, 1);
    }
    if(streaminfo.total_samples == 0) {
      fprintf(stderr, "Cannot load from an indeterminate-length FLAC (try FLACStream)\n");
      longjmp(fakethis->failbuf, 1);
    }
    if(streaminfo.channels == 2) {
      fakethis->ret = new StereoSoundBuffer(streaminfo.total_samples, streaminfo.sample_rate);
      fakethis->pos = (Sample*)(((StereoSoundBuffer*)fakethis->ret)->buffer);
    }
    else /* {streaminfo.channels == 1} */ {
      fakethis->ret = new MonoSoundBuffer(streaminfo.total_samples, streaminfo.sample_rate);
      fakethis->pos = ((MonoSoundBuffer*)fakethis->ret)->buffer;
    }
    fakethis->bits_per_sample = streaminfo.bits_per_sample;
    fakethis->channels = streaminfo.channels;
  }
}

static void callback_fail(const FLAC__StreamDecoder* flac, FLAC__StreamDecoderErrorStatus status, void* client_data) {
  FLACLoader* fakethis = (FLACLoader*)client_data;
  longjmp(fakethis->failbuf, 1);
}

SoundBuffer* FLACLoader::Load(const char* path) throw() {
  FILE* f = fopen(path, "rb");
  if(!f) return NULL;
  ret = NULL;
  if(setjmp(failbuf)) {
    FLAC__stream_decoder_finish(flac);
    if(ret) delete ret;
    //fclose(f); // FLAC__stream_decoder_finish fcloses for us, apparently
    return NULL;
  }
  if(FLAC__stream_decoder_init_FILE(flac, f, callback_write, callback_metadata, callback_fail, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    FLAC__stream_decoder_finish(flac);
    fclose(f);
    return NULL;
  }
  if(!FLAC__stream_decoder_process_until_end_of_stream(flac)) {
    fprintf(stderr, "unknown fatal FLAC error occurred\n");
    FLAC__stream_decoder_finish(flac);
    if(ret) delete ret;
    //fclose(f);
    return NULL;
  }
  FLAC__stream_decoder_finish(flac);
  //fclose(f);
  return ret;
}

PROTOCOL_IMP_PLAIN(FLACLoader, SoundLoader);

SUBCRITICAL_CONSTRUCTOR(FLACLoader)(lua_State* L) {
  (new FLACLoader())->Push(L);
  return 1;
}
