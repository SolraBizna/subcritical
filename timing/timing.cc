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
#include "subcritical/core.h"

#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <errno.h>

using namespace SubCritical;

#define NANOSECOND_UNSCALE ((double)0.000000001)
#define MICROSECOND_UNSCALE ((double)0.000001)
#define MILLISECOND_UNSCALE ((double)0.001)
#define NANOSECOND_SCALE ((double)1000000000.0)
#define MICROSECOND_SCALE ((double)1000000.0)
#define MILLISECOND_SCALE ((double)1000.0)

SUBCRITICAL_UTILITY(GetWallTime)(lua_State* L) {
#ifdef CLOCK_REALTIME
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  lua_pushnumber(L, ts.tv_sec + ts.tv_nsec * NANOSECOND_UNSCALE);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  lua_pushnumber(L, tv.tv_sec + tv.tv_usec * MICROSECOND_UNSCALE);
#endif
  return 1;
}

static double GetRelTime() {
  static double base = 0.0;
  double t;
#ifdef CLOCK_MONOTONIC
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  t = ts.tv_sec + ts.tv_nsec * NANOSECOND_UNSCALE;
#elif defined(CLOCK_REALTIME)
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  t = ts.tv_sec + ts.tv_nsec * NANOSECOND_UNSCALE;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  t = tv.tv_sec + tv.tv_usec * MICROSECOND_UNSCALE;
#endif
  if(!base) {
    base = t;
    return 0.0;
  }
  else return t - base;
}

SUBCRITICAL_UTILITY(GetTime)(lua_State* L) {
  lua_pushnumber(L, GetRelTime());
  return 1;
}

SUBCRITICAL_UTILITY(Sleep)(lua_State* L) {
  lua_Number length = luaL_checknumber(L, 1);
#ifdef TEN_MS_BUSYWAIT
  // Some operating systems (Linux, Windows?) can only wait about 10ms
  // resolution. Thus, if a very short wait was requested, we busywait.
  // See build.scb if you want to disable this.
  if(length <= 0.010) {
    double target = GetRelTime() + length, cur;
    do {
      cur = GetRelTime();
    } while(cur < target);
    return 0;
  }
#endif
#if _POSIX_C_SOURCE >= 199309L
  struct timespec ts, tr;
  ts.tv_sec = (time_t)floor(length);
  ts.tv_nsec = (long)floor((length - ts.tv_sec) * NANOSECOND_SCALE);
  do {
    if(nanosleep(&ts, &tr) && (errno == EINTR || errno == EAGAIN)) {
      ts.tv_sec = tr.tv_sec;
      ts.tv_nsec = tr.tv_nsec;
      continue;
    }
  } while(0);
#else //if (_BSD_SOURCE || _XOPEN_SOURCE >= 500)
#if !HAVE_WINDOWS
  if(length >= 1.0) {
    unsigned int rem = (unsigned int)floor(length);
    do { rem = sleep(rem); } while(rem > 0);
    length = length - floor(length);
  }
#endif
  usleep((unsigned int)(length * MICROSECOND_SCALE));
  /*#else
  if(length >= 1.0) {
    unsigned int rem = (unsigned int)floor(length);
    do { rem = sleep(rem); } while(rem > 0);
    length = length - floor(length);
  }
  msleep((unsigned int)(length * MILLISECOND_SCALE));*/
#endif
  return 0;
}
