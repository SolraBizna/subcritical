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

#ifndef _SUBCRITICAL_MUSIC_H
#define _SUBCRITICAL_MUSIC_H

#include "subcritical/sound.h"

namespace SubCritical {
  struct SampleBank;
  class EXPORT MusicMixer : public SoundStream {
    SampleBank** banks, *queued_bank, *front_bank;
    size_t max_banks, active_banks;
    Mutex lock;
    uint32_t framerate;
    enum {
      NoBranch,
      BranchNow,
      BranchOnFade,
      BranchOnEnd,
      BranchOnMeasure,
    } queued_branch_type;
    void TrimBanks() throw();
    bool TimeToBranch() throw();
  public:
    PROTOCOL_PROTOTYPE();
    MusicMixer(uint32_t framerate);
    virtual ~MusicMixer();
    virtual uint32_t GetFramerate() const throw();
    virtual void Mix(Frame* buffer, size_t count) throw();
    void QueueSampleBank(SoundBuffer** samples, size_t count,
                         uint32_t measure_count = 1,
                         lua_Number fade_out_time = 1.0,
                         lua_Number fade_in_time = 1.0,
                         bool delayed_branch = false,
                         bool branch_at_boundary = false);
    void Fade(lua_Number target_volume, lua_Number change_time);
    void SetSampleFadeTime(lua_Number change_time);
    void SetSampleTargetVolumes(lua_Number* volumes, size_t volume_count);
    uint32_t GetActiveBanks();
    inline bool IsBranchPending() const { return !!queued_bank; }
    int Lua_QueueSampleBank(lua_State* L) throw();
    int Lua_Fade(lua_State* L) throw();
    int Lua_SetSampleFadeTime(lua_State* L) throw();
    int Lua_SetSampleTargetVolumes(lua_State* L) throw();
    int Lua_GetActiveBanks(lua_State* L) throw();
    int Lua_IsBranchPending(lua_State* L) throw();
  };
}

#endif
