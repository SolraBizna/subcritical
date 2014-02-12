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
#include "sound.h"

// Get swab.
// PORT NOTE: On Windows, there is likely to be no swab. We'll have to
// implement its equivalent functionality on Windows. (However, it won't
// actually be called except on big-endian NT. Anyone here running NT on their
// Mac?)
#include <unistd.h>

#include <string.h>
#include <errno.h>

using namespace SubCritical;

struct wav_fmt_hdr {
  uint16_t magic; // 1
  uint16_t numchannels;
  uint32_t framerate, byterate;
  uint16_t bytesperframe, bitspersample;
};

class EXPORT WAVLoader : public SoundLoader {
 public:
  PROTOCOL_PROTOTYPE();
  virtual SoundBuffer* Load(const char* file) throw();
};

PROTOCOL_IMP_PLAIN(WAVLoader, SoundLoader);

static uint32_t ScanChunk(FILE* f, const char type[4]) {
  unsigned char buf[8];
  if(fread(buf, 8, 1, f) != 1) return 0;
  uint32_t size = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << (uint32_t)24);
  if(size == 0) return ScanChunk(f, type);
  else if(memcmp(buf, type, 4)) {
    if(fseek(f, size, SEEK_CUR)) {
      fprintf(stderr, "Warning: unseekable WAV with extra unhandled data\n");
      return 0;
    }
    return ScanChunk(f, type);
  }
  return size;
}

static void MixUp(unsigned char* src, Sample* dst, size_t samples) {
  UNROLL_MORE(samples,
	      *dst = (Sample)(uint16_t)(*src|((*src<<8)^0x8000)););
}

SoundBuffer* WAVLoader::Load(const char* file) throw() {
  FILE* f = fopen(file, "rb");
  if(!f) return NULL;
  {
    char buf[12];
    if(fread(buf, 12, 1, f) != 1) { fclose(f); return NULL; }
    if(memcmp(buf, "RIFF", 4) || memcmp(buf+8, "WAVE", 4)) { fclose(f); return NULL; }
  }
  uint32_t len = ScanChunk(f, "fmt ");
  if(len < 16) {
    fprintf(stderr, "WAVE with missing or unknown 'fmt ' chunk\n");
    fclose(f);
    return NULL;
  }
  struct wav_fmt_hdr hdr;
  if(fread(&hdr, sizeof(wav_fmt_hdr), 1, f) != 1) { fclose(f); return NULL; }
  if(!little_endian) {
    hdr.magic = Swap16(hdr.magic);
    hdr.numchannels = Swap16(hdr.numchannels);
    hdr.framerate = Swap32(hdr.framerate);
    hdr.byterate = Swap32(hdr.byterate);
    hdr.bytesperframe = Swap16(hdr.bytesperframe);
    hdr.bitspersample = Swap16(hdr.bitspersample);
  }
  if(hdr.magic != 1) {
    fprintf(stderr, "bad format magic %i in WAVE, not loading (unknown codec?)\n", hdr.magic);
    fclose(f);
    return NULL;
  }
  if(hdr.numchannels != 1 && hdr.numchannels != 2) {
    fprintf(stderr, "WAVE with %i channels not supported (only 1 and 2)\n", hdr.numchannels);
    fclose(f);
    return NULL;
  }
  if(!hdr.framerate) {
    fprintf(stderr, "WAVE with silly samplerate\n");
    fclose(f);
    return NULL;
  }
  if(hdr.bitspersample != 8 && hdr.bitspersample != 16) {
    fclose(f);
    fprintf(stderr, "WAVE with %i-bit audio not supported (only 8- and 16-bit)\n", hdr.bitspersample);
    return NULL;
  }
  len = ScanChunk(f, "data");
  if(len == 0) {
    fprintf(stderr, "WAVE with missing 'data' chunk\n");
    fclose(f);
    return NULL;
  }
  long savepos = ftell(f);
  if(savepos >= 0) {
    if(!fseek(f, 0, SEEK_END)) {
      long newpos = ftell(f);
      if(newpos >= 0) {
	uint32_t diff = (uint32_t)(newpos - savepos);
	if(diff < len) len = diff;
      }
      else fprintf(stderr, "A stream that we can ftell and then fseek but not ftell again afterwards? (Is this a huge WAV?)\n");
    }
    else fprintf(stderr, "A stream that we can ftell but not fseek?\n");
    fseek(f, savepos, SEEK_SET); // failure is NOT an option! :P
  }
  if(len == (uint32_t)0xFFFFFFFF) {
    fprintf(stderr, "Reading an unbounded-length WAVE stream from a pipe is not supported.\nTry WAVStream instead.\n");
    fclose(f);
    return NULL;
  }
  SoundBuffer* ret;
  size_t samples = len / (hdr.bitspersample == 8 ? 1 : 2);
  Sample* p;
  if(hdr.numchannels == 1) {
    try {
      ret = new MonoSoundBuffer(samples, hdr.framerate);
    }
    catch(...) {
      fclose(f);
      return NULL;
    }
    p = ((MonoSoundBuffer*)ret)->buffer;
  }
  else /* {hdr.numchannels == 2} */ {
    try {
      ret = new StereoSoundBuffer(samples/2, hdr.framerate);
    }
    catch(...) {
      fclose(f);
      return NULL;
    }
    p = (Sample*)((StereoSoundBuffer*)ret)->buffer;
  }
  if(little_endian && hdr.bitspersample == 16) {
    // No fancy twiddling necessary
    if(fread(p, sizeof(Sample), samples, f) != samples) {
      fprintf(stderr, "Early EOF on WAVE\n");
      delete ret;
      fclose(f);
      return NULL;
    }
  }
#define BUFLEN 1024
  else if(hdr.bitspersample == 16) {
    // Need to endian swap
    size_t rem = samples;
    Sample buf[BUFLEN];
    while(rem > 0) {
      size_t read = fread(buf, sizeof(Sample), rem > BUFLEN ? BUFLEN : rem, f);
      if(read == 0) {
	fprintf(stderr, "Early EOF on WAVE\n");
	delete ret;
	fclose(f);
	return NULL;
      }
      swab((const char*)buf, (char*)p, read*2);
      rem -= read;
      p += read;
    }
  }
  else /* {fmt.bitspersample == 8} */ {
    size_t rem = samples;
    unsigned char buf[BUFLEN];
    while(rem > 0) {
      size_t read = fread(buf, 1, rem > BUFLEN ? BUFLEN : rem, f);
      if(read == 0) {
	fprintf(stderr, "Early EOF on WAVE\n");
	delete ret;
	fclose(f);
	return NULL;
      }
      MixUp(buf, p, read);
      rem -= read;
      p += read;
    }
  }
  return ret;
}

SUBCRITICAL_CONSTRUCTOR(WAVLoader)(lua_State* L) {
  (new WAVLoader())->Push(L);
  return 1;
}

class EXPORT WAVStream : public SoundStream {
 public:
  PROTOCOL_PROTOTYPE();
  virtual ~WAVStream();
  WAVStream(const char* path) throw(const char*);
  virtual uint32_t GetFramerate() const throw();
  virtual void Mix(Frame* buffer, size_t length) throw();
 private:
  FILE* f;
  struct wav_fmt_hdr hdr;
  long savepos;
  uint32_t savesamples, remsamples;
};

WAVStream::WAVStream(const char* path) throw(const char*) {
  f = fopen(path, "rb");
  if(!f) throw (const char*)strerror(errno);
  {
    char buf[12];
    if(fread(buf, 12, 1, f) != 1) { fclose(f); throw "Way too short to be a WAV file"; }
    if(memcmp(buf, "RIFF", 4) || memcmp(buf+8, "WAVE", 4)) { fclose(f); throw "Not a WAVE file"; }
  }
  uint32_t len = ScanChunk(f, "fmt ");
  if(len < 16) {
    fclose(f);
    throw "WAVE with missing or unknown 'fmt ' chunk";
  }
  if(fread(&hdr, sizeof(wav_fmt_hdr), 1, f) != 1) { fclose(f); throw "early EOF reading 'fmt ' chunk"; }
  if(!little_endian) {
    hdr.magic = Swap16(hdr.magic);
    hdr.numchannels = Swap16(hdr.numchannels);
    hdr.framerate = Swap32(hdr.framerate);
    hdr.byterate = Swap32(hdr.byterate);
    hdr.bytesperframe = Swap16(hdr.bytesperframe);
    hdr.bitspersample = Swap16(hdr.bitspersample);
  }
  if(hdr.magic != 1) {
    fclose(f);
    throw "bad format magic in WAVE, not loading (unknown codec?)";
  }
  if(hdr.numchannels != 1 && hdr.numchannels != 2) {
    fclose(f);
    throw "only mono and stereo WAVE are supported";
  }
  if(!hdr.framerate) {
    fclose(f);
    throw "WAVE with silly samplerate";
  }
  if(hdr.bitspersample != 8 && hdr.bitspersample != 16) {
    fclose(f);
    throw "only 8- and 16-bit WAVE are supported";
  }
  len = ScanChunk(f, "data");
  if(len == 0) {
    fclose(f);
    throw "WAVE with missing 'data' chunk";
  }
  savepos = ftell(f);
  if(savepos == -1) fprintf(stderr, "WARNING: Non-seekable WAVE stream. If this stream ends, BAD things will happen.\n");
  savesamples = len / (hdr.bitspersample == 8 ? 1 : 2);
  remsamples = savesamples;
}

WAVStream::~WAVStream() {
  if(f) {
    fclose(f);
    f = NULL;
  }
}

uint32_t WAVStream::GetFramerate() const throw() {
  return hdr.framerate;
}

static void DoubleUp(Sample* p, size_t mono_count) {
  for(int32_t n = mono_count-1; n >= 0; --n) {
    p[n*2+1] = p[n*2] = p[n];
  }
}

void WAVStream::Mix(Frame* buffer, size_t out_count) throw() {
  Sample* sbuffer = (Sample*)buffer;
  size_t out_samps = out_count;
  size_t read;
  if(hdr.numchannels == 2) out_samps *= 2;
  if(out_samps > remsamples) out_samps = remsamples;
  if(little_endian && hdr.bitspersample == 16) {
    // No fancy twiddling necessary
    read = fread(sbuffer, sizeof(Sample), out_samps, f);
  }
#define BUFLEN 1024
  else if(hdr.bitspersample == 16) {
    Sample aux[out_samps];
    read = fread(aux, sizeof(Sample), out_samps, f);
    swab((const char*)aux, (char*)sbuffer, read*2);
  }
  else /* {fmt.bitspersample == 8} */ {
    uint8_t aux[out_samps];
    read = fread(aux, 1, out_samps, f);
    MixUp(aux, sbuffer, read);
  }
  remsamples -= read;
  if(hdr.numchannels == 1) DoubleUp(sbuffer, read);
  else read /= 2;
  if(feof(f) || remsamples == 0) {
    remsamples = savesamples;
    fseek(f, savepos, SEEK_SET);
  }
  if(read < out_count)
    Mix(buffer + read, out_count - read);
}

SUBCRITICAL_CONSTRUCTOR(WAVStream)(lua_State* L) {
  try {
    (new WAVStream(GetPath(L,1)))->Push(L);
    return 1;
  }
  catch(const char* e) {
    lua_pushnil(L);
    lua_pushstring(L, e);
    return 2;
  }
}

PROTOCOL_IMP_PLAIN(WAVStream, SoundStream);
