#include "music.h"

#include <new>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

using namespace SubCritical;

static inline int16_t Volume2Q(lua_Number volume) {
  if(volume > 1.0) return 4096;
  else if(volume < -1.0) return -4096;
  else return (int16_t)(volume * 4096);
}

struct SubCritical::SampleBank {
  // keep these around for mind-changing
  SoundBuffer** samples;
  struct VolumeDDA {
    int16_t cur_volume, target_volume; // Q3.12
    int32_t accum;
    VolumeDDA(int16_t volume) : cur_volume(volume), target_volume(volume), accum(0) {}
    inline int16_t Step(int32_t num, int32_t den) {
      if(cur_volume == target_volume) return cur_volume;
      accum = accum + num;
      if(accum >= den) {
        if(den == 0) {
          /* this check goes here so it doesn't incur overhead needlessly */
          goto skip_to_the_end;
        }
        int16_t dir;
        if(cur_volume < target_volume) dir = 1;
        else dir = -1;
        cur_volume += dir * (accum / den);
        accum = accum % den;
        int16_t nudir;
        if(cur_volume == target_volume)
          nudir = 0;
        else if(cur_volume < target_volume)
          nudir = 1;
        else
          nudir = -1;
        if(dir != nudir) {
          /* we crossed the target */
        skip_to_the_end:
          accum = 0;
          cur_volume = target_volume;
        }
      }
      return cur_volume;
    }
  }* sample_volumes, bank_volume;
  struct SampleInfo {
    bool stereo;
    Sample* buffer, *end;
    Sample* pos;
  }* sample_infos;
  size_t sample_count;
  uint32_t framerate;
  uint32_t bank_change_time, sample_change_time;
  uint32_t pos, length, interrupt_rate;
  SampleBank(SoundBuffer** samples, size_t count, uint32_t framerate,
             uint32_t measure_count) :
    bank_volume(4096), sample_count(count), framerate(framerate),
    bank_change_time(framerate), sample_change_time(framerate),
    pos(0)
  {
    this->samples = (SoundBuffer**)malloc(sizeof(SoundBuffer*) * count);
    if(!this->samples) throw std::bad_alloc();
    sample_volumes = (VolumeDDA*)malloc(sizeof(VolumeDDA) * count);
    if(!sample_volumes) {
      free(this->samples);
      throw std::bad_alloc();
    }
    sample_infos = (SampleInfo*)malloc(sizeof(SampleInfo) * count);
    if(!sample_infos) {
      free(this->samples);
      free(this->sample_volumes);
      throw std::bad_alloc();
    }
    if(count > 0) {
      new(sample_volumes) VolumeDDA(4096);
      for(unsigned n = 1; n < count; ++n)
        new(sample_volumes + n) VolumeDDA(0);
      for(unsigned n = 0; n < count; ++n) {
        SampleInfo& info = sample_infos[n];
        if(samples[n]->IsA("StereoSoundBuffer")) {
          StereoSoundBuffer* sample = (StereoSoundBuffer*)samples[n];
          info.stereo = true;
          info.buffer = (Sample*)sample->buffer;
          info.end = info.buffer + sample->frames * 2;
          info.pos = info.buffer;
          if(n == 0) length = sample->frames;
        }
        else {
          MonoSoundBuffer* sample = (MonoSoundBuffer*)samples[n];
          info.stereo = false;
          info.buffer = (Sample*)sample->buffer;
          info.end = info.buffer + sample->frames;
          info.pos = info.buffer;
          if(n == 0) length = sample->frames;
        }
      }
      if(length == 0) length = 1;
    }
    else {
      interrupt_rate = 1;
      length = 1;
    }
    interrupt_rate = length / measure_count;
    if(interrupt_rate == 0) interrupt_rate = 1;
    memcpy(this->samples, samples, sizeof(SoundBuffer*) * count);
  }
  ~SampleBank() {
    if(samples) {
      free(samples);
      samples = NULL;
    }
    if(sample_volumes) {
      free(sample_volumes);
      sample_volumes = NULL;
    }
    if(sample_infos) {
      free(sample_infos);
      sample_infos = NULL;
    }
  }
  bool Equivalent(SoundBuffer** samples, size_t count) const {
    if(this->sample_count != count) return false;
    for(unsigned n = 0; n < count; ++n) {
      if(samples[n] != this->samples[n]) return false;
    }
    return true;
  }
  size_t Mix(Frame* buffer, size_t count, uint32_t interrupt) throw();
  inline bool IsSilenced() const throw() {
    return bank_volume.cur_volume == 0 && bank_volume.target_volume == 0;
  }
  void ApplyVolumes(lua_Number* volumes, size_t volume_count) {
    if(volume_count > sample_count) volume_count = sample_count;
    for(unsigned n = 0; n < volume_count; ++n) {
      sample_volumes[n].target_volume = Volume2Q(volumes[n]);
    }
    for(unsigned n = volume_count; n < sample_count; ++n) {
      sample_volumes[n].target_volume = 0;
    }
  }
};

size_t SampleBank::Mix(Frame* buffer, size_t count, uint32_t interrupt) throw() {
  if(sample_count == 0) return 0;
  size_t outputted = 0;
  while(count-- > 0) {
    if(interrupt && pos % interrupt == 0)
      return outputted;
    int32_t bank_v = bank_volume.Step(4096, bank_change_time);
    // if we're silenced, we're about to be trimmed and shouldn't bother to
    // continue updating our samples' states
    if(bank_v == 0 && bank_volume.target_volume == 0) return outputted;
    for(unsigned n = 0; n < sample_count; ++n) {
      SampleInfo& info = sample_infos[n];
      int32_t sample_v = sample_volumes[n].Step(4096, sample_change_time);
      if(sample_v != 0) {
        sample_v = (sample_v * bank_v) >> 12;
        int16_t left, right;
        if(info.stereo) {
          left = (*info.pos++ * sample_v) >> 12;
          right = (*info.pos++ * sample_v) >> 12;
        }
        else {
          right = left = (*info.pos++ * sample_v) >> 12;
        }
        (*buffer)[0] += left;
        (*buffer)[1] += right;
        if(info.pos == info.end) info.pos = info.buffer;
      }
      else {
        info.pos += info.stereo ? 2 : 1;
        if(info.pos == info.end) info.pos = info.buffer;
      }
    }
    ++pos;
    ++buffer;
    ++outputted;
    if(pos >= length) pos -= length;
  }
  return outputted;
}

uint32_t MusicMixer::GetFramerate() const throw() {
  return framerate;
}

void MusicMixer::TrimBanks() throw() {
  if(active_banks == 0) return;
  /* never trim the first bank */
  unsigned m = 1;
  for(unsigned n = 1; n < active_banks; ++n) {
    if(banks[n]->IsSilenced() || banks[n]->sample_count == 0)
      delete banks[n];
    else
      banks[m++] = banks[n];
  }
  active_banks = m;
}

bool MusicMixer::TimeToBranch() throw() {
  if(active_banks == 0 || banks[0]->sample_count == 0 || banks[0]->IsSilenced()) return true;
  switch(queued_branch_type) {
  default:
  case BranchNow: return true;
  case BranchOnFade: return false;
  case BranchOnEnd: return banks[0]->pos == 0;
  case BranchOnMeasure: return banks[0]->pos % banks[0]->interrupt_rate == 0;
  }
  /* :| */
  return true;
}

void MusicMixer::Mix(Frame* buffer, size_t count) throw() {
  memset(buffer, 0, sizeof(Frame)*count);
  lock.Lock();
  if(queued_bank && TimeToBranch()) {
    if(active_banks) {
      banks[0]->bank_volume.target_volume = 0;
    }
    /* insert queued_bank at the front of active_banks */
    ++active_banks;
    if(active_banks > max_banks) {
      max_banks = active_banks;
      banks = (SampleBank**)realloc(banks, sizeof(SampleBank*)*max_banks);
      if(!banks) throw std::bad_alloc(); // didn't we say we wouldn't throw?
    }
    for(unsigned n = active_banks - 1; n > 0; --n) {
      banks[n] = banks[n-1];
    }
    banks[0] = queued_bank;
    queued_bank = NULL;
    queued_branch_type = NoBranch;
  }
  size_t mixed = count;
  if(active_banks > 0) switch(queued_branch_type) {
  case BranchNow: /* NOTREACHED */
  default:
  case NoBranch:
    for(unsigned n = 0; n < active_banks; ++n) {
      banks[n]->Mix(buffer, count, 0);
    }
    break;
  case BranchOnFade:
    mixed = banks[0]->Mix(buffer, count, 0);
    for(unsigned n = 1; n < active_banks; ++n) {
      banks[n]->Mix(buffer, mixed, 0);
    }
    break;
  case BranchOnEnd:
    mixed = banks[0]->Mix(buffer, count, banks[0]->length);
    for(unsigned n = 1; n < active_banks; ++n) {
      banks[n]->Mix(buffer, mixed, 0);
    }
    break;
  case BranchOnMeasure:
    mixed = banks[0]->Mix(buffer, count, banks[0]->interrupt_rate);
    for(unsigned n = 1; n < active_banks; ++n) {
      banks[n]->Mix(buffer, mixed, 0);
    }
    break;
  }
  TrimBanks();
  lock.Unlock();
  count -= mixed;
  buffer += mixed;
  if(count > 0) return Mix(buffer, count);
}

void MusicMixer::QueueSampleBank(SoundBuffer** samples, size_t sample_count,
                                 uint32_t measure_count,
                                 lua_Number _fade_out_time,
                                 lua_Number _fade_in_time,
                                 bool delayed_branch,
                                 bool branch_at_boundary) {
  if(branch_at_boundary) delayed_branch = false;
  lock.Lock();
  uint32_t fade_out_time = (uint32_t)(_fade_out_time * framerate);
  uint32_t fade_in_time = (uint32_t)(_fade_in_time * framerate);
  /* first, set up the fade outs. */
  for(unsigned n = 0; n < active_banks; ++n) {
    SampleBank& bank = *banks[n];
    /* if the bank was already fading to 0 at a faster rate, don't slow it */
    /* assert(n == 0 && bank.bank_volume.target_volume == 0) */
    if(bank.bank_volume.target_volume == 0 && bank.bank_change_time < fade_out_time) continue;
    if(n != 0 || delayed_branch)
      bank.bank_volume.target_volume = 0;
    if(fade_out_time != bank.bank_change_time) {
      bank.bank_change_time = fade_out_time;
      bank.bank_volume.accum = 0;
    }
  }
  /* now, check to see if this bank is already present */
  SampleBank** renew_target = NULL;
  for(unsigned n = 0; n < active_banks; ++n) {
    if(banks[n]->Equivalent(samples, sample_count)) {
      renew_target = banks + n;
      break;
    }
  }
  if(renew_target) {
    SampleBank* old_front = banks[0];
    front_bank = banks[0] = *renew_target;
    *renew_target = old_front;
    old_front->bank_volume.target_volume = 0;
    /* if a bank was already queued, it should get preempted (even though we
       don't intend to put another bank directly in its place) */
    if(queued_bank) {
      delete queued_bank;
      queued_bank = NULL;
      queued_branch_type = NoBranch;
    }
  }
  else if(queued_bank && queued_bank->Equivalent(samples, sample_count)) {
    /* do nothing; the requested SampleBank is already queued */
  }
  else {
    if(queued_bank) {
      delete queued_bank;
      queued_bank = NULL;
      queued_branch_type = NoBranch;
    }
    queued_bank = new SampleBank(samples, sample_count, framerate, measure_count);
    front_bank = queued_bank;
  }
  if(fade_in_time) {
    front_bank->bank_volume.cur_volume = 0;
    front_bank->bank_volume.target_volume = 4096;
    if(front_bank->bank_change_time != fade_in_time) {
      front_bank->bank_change_time = fade_in_time;
      front_bank->bank_volume.accum = 0;
    }
  }
  if(renew_target) {
    /* the resulting SampleBank is already active, no branch needed */
    queued_branch_type = NoBranch;
  }
  else {
    if(branch_at_boundary)
      queued_branch_type = BranchOnMeasure;
    else if(delayed_branch) {
      if(fade_out_time)
        queued_branch_type = BranchOnFade;
      else
        queued_branch_type = BranchOnEnd;
    }
    else queued_branch_type = BranchNow;
  }
  lock.Unlock();
}

void MusicMixer::Fade(lua_Number _target_volume, lua_Number _change_time) {
  int16_t target_volume = Volume2Q(_target_volume);
  uint32_t change_time = (uint32_t)(_change_time * framerate);
  lock.Lock();
  if(front_bank) {
    front_bank->bank_volume.target_volume = target_volume;
    if(change_time != front_bank->bank_change_time) {
      front_bank->bank_change_time = change_time;
      front_bank->bank_volume.accum = 0;
    }
  }
  lock.Unlock();
}

void MusicMixer::SetSampleFadeTime(lua_Number _change_time) {
  uint32_t change_time = (uint32_t)(_change_time * framerate);
  lock.Lock();
  if(front_bank) {
    if(change_time != front_bank->sample_change_time) {
      front_bank->sample_change_time = change_time;
      for(unsigned n = 0; n < front_bank->sample_count; ++n) {
        front_bank->sample_volumes[n].accum = 0;
      }
    }
  }
  lock.Unlock();
}

void MusicMixer::SetSampleTargetVolumes(lua_Number* volumes, size_t volume_count) {
  lock.Lock();
  if(front_bank)
    front_bank->ApplyVolumes(volumes, volume_count);
  lock.Unlock();
}

void MusicMixer::SetAllSampleTargetVolumes(lua_Number* volumes, size_t volume_count) {
  lock.Lock();
  if(queued_bank)
    queued_bank->ApplyVolumes(volumes, volume_count);
  for(unsigned n = 0; n < active_banks; ++n)
    banks[n]->ApplyVolumes(volumes, volume_count);
  lock.Unlock();
}

uint32_t MusicMixer::GetActiveBanks() {
  uint32_t ret;
  lock.Lock();
  if(active_banks) {
    if(banks[0]->sample_count == 0) ret = active_banks - 1;
    else ret = active_banks;
  }
  else ret = 0;
  if(queued_bank) ++ret;
  lock.Unlock();
  return ret;
}

MusicMixer::MusicMixer(uint32_t framerate) :
  banks(NULL), queued_bank(NULL), front_bank(NULL), max_banks(0),
  active_banks(0), framerate(framerate), queued_branch_type(NoBranch) {}

MusicMixer::~MusicMixer() {
  lock.Lock();
  if(queued_bank) {
    delete queued_bank;
    queued_bank = NULL;
  }
  if(banks) {
    for(unsigned n = 0; n < active_banks; ++n) {
      if(banks[n]) {
        delete banks[n];
        banks[n] = NULL;
      }
    }
    free(banks);
    banks = NULL;
  }
  lock.Unlock();
}

int MusicMixer::Lua_QueueSampleBank(lua_State* L) throw() {
  luaL_checktype(L, 1, LUA_TTABLE);
  unsigned count = lua_rawlen(L, 1);
  SoundBuffer* samples[count];
  for(unsigned i = 0; i < count; ++i) {
    lua_pushinteger(L, i+1);
    lua_gettable(L, 1);
    samples[i] = lua_toobject(L, -1, SoundBuffer);
    lua_pop(L, 1);
  }
  uint32_t measure_count = 1;
  lua_getfield(L, 1, "measure_count");
  if(!lua_isnil(L,-1)) {
    lua_Integer n = luaL_checkinteger(L, -1);
    if(n < 1) return luaL_error(L, "measure_count must be >= 1");
    measure_count = n;
  }
  lua_pop(L, 1);
  lua_Number fade_out_time = 0.0;
  lua_Number fade_in_time = 0.0;
  bool delayed_branch = false;
  bool branch_at_boundary = false;
  if(lua_gettop(L) >= 2) {
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "fade_out_time");
    if(!lua_isnil(L,-1)) fade_out_time = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "fade_in_time");
    if(!lua_isnil(L,-1)) fade_in_time = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "delayed_branch");
    if(!lua_isnil(L,-1)) delayed_branch = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 2, "branch_at_boundary");
    if(!lua_isnil(L,-1)) branch_at_boundary = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  QueueSampleBank(samples, count, measure_count, fade_out_time, fade_in_time, delayed_branch, branch_at_boundary);
  return 0;
}

int MusicMixer::Lua_Fade(lua_State* L) throw() {
  lua_Number target_volume = luaL_optnumber(L, 1, 0.0);
  lua_Number change_time = luaL_optnumber(L, 2, 1.0);
  Fade(target_volume, change_time);
  return 0;
}

int MusicMixer::Lua_SetSampleFadeTime(lua_State* L) throw() {
  lua_Number change_time = luaL_checknumber(L, 1);
  SetSampleFadeTime(change_time);
  return 0;
}

int MusicMixer::Lua_SetSampleTargetVolumes(lua_State* L) throw() {
  unsigned count = lua_gettop(L);
  if(count == 1) {
    if(lua_istable(L, 1)) {
      count = lua_rawlen(L, 1);
      lua_Number targets[count];
      for(unsigned i = 0; i < count; ++i) {
        lua_pushinteger(L, i+1);
        lua_gettable(L, 1);
        targets[i] = luaL_checknumber(L, -1);
        lua_settop(L, 1);
      }
      SetSampleTargetVolumes(targets, count);
    }
    else {
      lua_Number target = luaL_checknumber(L, 1);
      SetSampleTargetVolumes(&target, 1);
    }
  }
  else {
    lua_Number targets[count];
    for(unsigned i = 0; i < count; ++i) {
      targets[i] = luaL_checknumber(L, i+1);
    }
    SetSampleTargetVolumes(targets, count);
  }
  return 0;
}

int MusicMixer::Lua_SetAllSampleTargetVolumes(lua_State* L) throw() {
  unsigned count = lua_gettop(L);
  if(count == 1) {
    if(lua_istable(L, 1)) {
      count = lua_rawlen(L, 1);
      lua_Number targets[count];
      for(unsigned i = 0; i < count; ++i) {
        lua_pushinteger(L, i+1);
        lua_gettable(L, 1);
        targets[i] = luaL_checknumber(L, -1);
        lua_settop(L, 1);
      }
      SetAllSampleTargetVolumes(targets, count);
    }
    else {
      lua_Number target = luaL_checknumber(L, 1);
      SetAllSampleTargetVolumes(&target, 1);
    }
  }
  else {
    lua_Number targets[count];
    for(unsigned i = 0; i < count; ++i) {
      targets[i] = luaL_checknumber(L, i+1);
    }
    SetAllSampleTargetVolumes(targets, count);
  }
  return 0;
}

int MusicMixer::Lua_GetActiveBanks(lua_State* L) throw() {
  lua_pushinteger(L, GetActiveBanks());
  return 1;
}

int MusicMixer::Lua_IsBranchPending(lua_State* L) throw() {
  lua_pushboolean(L, IsBranchPending());
  return 1;
}

static const struct ObjectMethod MMMethods[] = {
  METHOD("QueueSampleBank", &MusicMixer::Lua_QueueSampleBank),
  METHOD("Fade", &MusicMixer::Lua_Fade),
  METHOD("SetSampleFadeTime", &MusicMixer::Lua_SetSampleFadeTime),
  METHOD("SetSampleTargetVolumes", &MusicMixer::Lua_SetSampleTargetVolumes),
  METHOD("SetAllSampleTargetVolumes", &MusicMixer::Lua_SetAllSampleTargetVolumes),
  METHOD("GetActiveBanks", &MusicMixer::Lua_GetActiveBanks),
  METHOD("IsBranchPending", &MusicMixer::Lua_IsBranchPending),
  NOMOREMETHODS(),
};

PROTOCOL_IMP(MusicMixer, SoundStream, MMMethods);

SUBCRITICAL_CONSTRUCTOR(MusicMixer)(lua_State* L) {
  lua_Integer framerate = luaL_checkinteger(L, 1);
  (new MusicMixer(framerate))->Push(L);
  return 1;
}
