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

#include "subcritical/sound.h"

#include <new> // for bad_alloc
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

using namespace SubCritical;

#include "FLAC/stream_decoder.h"

static FLAC__StreamDecoderWriteStatus callback_write(const FLAC__StreamDecoder* flac, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
static void callback_metadata(const FLAC__StreamDecoder* flac, const FLAC__StreamMetadata* metadata, void* client_data);
static void callback_fail(const FLAC__StreamDecoder* flac, FLAC__StreamDecoderErrorStatus status, void* client_data);

class FLACStream : public SoundStream {
public:
  PROTOCOL_PROTOTYPE();
  virtual ~FLACStream();
  FLACStream(const char* file);
  virtual uint32_t GetFramerate() const throw();
  virtual void Mix(Frame* out, size_t count) throw();
  virtual int Lua_GetTags(lua_State* L) const throw();
private:
  FLAC__StreamDecoder* flac;
  FILE* f;
  uint32_t framerate;
  int bits_per_sample, channels;
  uint64_t loop_left, loop_right, pos;
  uint32_t comments;
  char* commentbuf;
  char** commentps;
  bool failed;
  Sample* buf;
  uint32_t buffill, bufpos;
  friend FLAC__StreamDecoderWriteStatus callback_write(const FLAC__StreamDecoder* flac, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
  friend void callback_metadata(const FLAC__StreamDecoder* flac, const FLAC__StreamMetadata* metadata, void* client_data);
  friend void callback_fail(const FLAC__StreamDecoder* flac, FLAC__StreamDecoderErrorStatus status, void* client_data);
  jmp_buf failbuf;
};

uint32_t FLACStream::GetFramerate() const throw() {
  return framerate;
}

FLACStream::~FLACStream() {
  FLAC__stream_decoder_delete(flac);
  if(buf) free(buf);
  if(commentbuf) free(commentbuf);
}

static inline Sample EightToSixteen(FLAC__int32 eight) {
  return ((eight) << 8) | (0x80^(uint8_t)eight);
}

static void DoubleOut(Frame* dest, const Sample* src, size_t mono_count) {
  UNROLL_MORE(mono_count,
	      (*dest)[1] = (*dest)[0] = *src;
	      ++dest; ++src;);
}

static FLAC__StreamDecoderWriteStatus callback_write(const FLAC__StreamDecoder* flac, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data) {
  FLACStream* fakethis = (FLACStream*)client_data;
  if(!fakethis->buf) {
    fprintf(stderr, "Audio frame before STREAMINFO! Ouch!\n");
    longjmp(fakethis->failbuf, 2);
  }
  if(fakethis->buffill) {
    fprintf(stderr, "We read another audio frame before we finished with our previous one! Ouch!\n");
    longjmp(fakethis->failbuf, 3);
  }
  fakethis->bufpos = 0;
  size_t rem = fakethis->buffill = frame->header.blocksize;
  size_t i = 0;
  Sample* p = fakethis->buf;
  if(fakethis->bits_per_sample == 16) {
    if(fakethis->channels == 2)
      UNROLL_MORE(rem,
		  *p++ = buffer[0][i];
		  *p++ = buffer[1][i];
		  ++i;);
    else /* {fakethis->channels == 1} */
      UNROLL_MORE(rem,
		  *p++ = buffer[0][i++];);
  }
  else /* {fakethis->bits_per_sample == 8} */ {
    if(fakethis->channels == 2)
      UNROLL_MORE(rem,
		  *p++ = EightToSixteen(buffer[0][i]);
		  *p++ = EightToSixteen(buffer[1][i]);
		  ++i;);
    else /* {fakethis->channels == 1} */
      UNROLL_MORE(rem,
		  *p++ = EightToSixteen(buffer[0][i++]););
  }
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void callback_metadata(const FLAC__StreamDecoder* flac, const FLAC__StreamMetadata* metadata, void* client_data) {
  FLACStream* fakethis = (FLACStream*)client_data;
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
    fakethis->bits_per_sample = streaminfo.bits_per_sample;
    fakethis->channels = streaminfo.channels;
    fakethis->framerate = streaminfo.sample_rate;
    // The largest possible decoded buffer.
    fakethis->buf = (Sample*)malloc(streaminfo.max_blocksize*streaminfo.channels*streaminfo.bits_per_sample/8);
    if(streaminfo.total_samples) fakethis->loop_right = streaminfo.total_samples; // No easy way to distinguish between EOF and error -_-
  }
  else if(metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
    const FLAC__StreamMetadata_VorbisComment& vc = metadata->data.vorbis_comment;
    if(vc.num_comments) {
      if(fakethis->comments) {
	fprintf(stderr, "Warning: Multiple non-empty VORBIS_COMMENT blocks; ignoring all but the first\n");
	return;
      }
      size_t total_size = sizeof(char*) * vc.num_comments;
      fakethis->comments = vc.num_comments;
      for(uint32_t n = 0; n < vc.num_comments; ++n) {
	total_size += vc.comments[n].length + 1;
      }
      fakethis->commentbuf = (char*)malloc(total_size);
      char** pp = (char**)fakethis->commentbuf;
      char* p = fakethis->commentbuf + sizeof(char*) * vc.num_comments;
      fakethis->commentps = pp;
      for(uint32_t n = 0; n < vc.num_comments; ++n) {
	*pp++ = p;
	strcpy(p, (const char*)vc.comments[n].entry);
	if(!strncasecmp(p, "LOOP_START=", 11)) {
	  double t;
	  char* e;
	  t = strtod(p + 11, &e);
	  if(e && *e) {
	    fprintf(stderr, "WARNING: Invalid LOOP_START tag\n");
	    continue;
	  }
	  fakethis->loop_left = (uint64_t)(t * fakethis->framerate);
	}
	else if(!strncasecmp(p, "LOOP_END=", 9)) {
	  double t;
	  char* e;
	  t = strtod(p + 9, &e);
	  if(e && *e) {
	    fprintf(stderr, "WARNING: Invalid LOOP_END tag\n");
	    continue;
	  }
	  fakethis->loop_right = (uint64_t)(t * fakethis->framerate);
	}
	p += vc.comments[n].length + 1;
      }
    }
  }
}

static void callback_fail(const FLAC__StreamDecoder* flac, FLAC__StreamDecoderErrorStatus status, void* client_data) {
  FLACStream* fakethis = (FLACStream*)client_data;
  longjmp(fakethis->failbuf, 1);
}

FLACStream::FLACStream(const char* path) :
  loop_left(0), loop_right(~(uint64_t)0), pos(0), comments(0), commentbuf(0), failed(false), buf(NULL), buffill(0) {
  f = fopen(path, "rb");
  if(!f) throw (const char*)"Could not open file";
  flac = FLAC__stream_decoder_new();
  if(!flac) throw std::bad_alloc();
  if(setjmp(failbuf)) {
    FLAC__stream_decoder_finish(flac);
    FLAC__stream_decoder_delete(flac);
    throw (const char*)"libFLAC metadata error";
  }
  FLAC__stream_decoder_set_metadata_respond(flac, FLAC__METADATA_TYPE_VORBIS_COMMENT);
  if(FLAC__stream_decoder_init_FILE(flac, f, callback_write, callback_metadata, callback_fail, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    FLAC__stream_decoder_finish(flac);
    FLAC__stream_decoder_delete(flac);
    fclose(f);
    throw (const char*)"libFLAC setup error";
  }
  if(!FLAC__stream_decoder_process_until_end_of_metadata(flac)) {
    fprintf(stderr, "unknown fatal FLAC error occurred\n");
    FLAC__stream_decoder_finish(flac);
    FLAC__stream_decoder_delete(flac);
    throw (const char*)"libFLAC metadata error";
  }
}

void FLACStream::Mix(Frame* out, size_t count) throw() {
  if(failed) return;
  if(setjmp(failbuf)) {
    fprintf(stderr, "failbuf got longjmp'd... some kind of FLAC error.\n");
    failed = true;
    return;
  }
  if(pos == loop_right) {
    pos = loop_left;
    buffill = 0;
    if(!FLAC__stream_decoder_seek_absolute(flac, loop_left)) {
      fprintf(stderr, "Seek-lap failed. Non-seekable FLAC? Faulty LOOP_START tag?\n");
      failed = true;
      return;
    }
  }
  size_t read_count = count;
  if(read_count + pos > loop_right)
    read_count = loop_right - pos;
  while(!buffill || (buffill == bufpos)) {
    buffill = 0;
    if(!FLAC__stream_decoder_process_single(flac)) {
      fprintf(stderr, "Some kind of FLAC error.\n");
      failed = true;
      return;
    }
  }
  if(read_count > buffill - bufpos) read_count = buffill - bufpos;
  if(read_count == 0) exit(0);
  if(channels == 1)
    DoubleOut(out, buf + bufpos, read_count);
  else
    memcpy(out, buf + bufpos*2, read_count*sizeof(Frame));
  out += read_count;
  count -= read_count;
  bufpos += read_count;
  pos += read_count;
  if(count > 0) Mix(out, count);
}


static const struct ObjectMethod methods[] = {
  METHOD("GetTags", &FLACStream::Lua_GetTags),
  NOMOREMETHODS(),
};

PROTOCOL_IMP(FLACStream, SoundStream, methods);

int FLACStream::Lua_GetTags(lua_State* L) const throw() {
  lua_createtable(L, comments, 0);
  if(comments) {
    for(uint32_t n = 0; n < comments; ++n) {
      lua_pushstring(L, commentps[n]);
      lua_rawseti(L, -2, n+1);
    }
  }
  return 1;
}

SUBCRITICAL_CONSTRUCTOR(FLACStream)(lua_State* L) {
  try {
    (new FLACStream(GetPath(L,1)))->Push(L);
    return 1;
  }
  catch(const char* e) {
    lua_pushnil(L);
    lua_pushstring(L, e);
    return 2;
  }
}
