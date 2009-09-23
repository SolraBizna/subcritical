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
#include "sdlman.h"

static bool init_failed = false;
static Uint32 init_systems = 0;
static bool inited = false;

void SDLMan::InitializeSubsystem(Uint32 system) {
  if(init_failed) throw (const char*)"Initialization already failed once";
  if(!inited) {
    if(SDL_Init(0)) {
      init_failed = true;
      throw (const char*)SDL_GetError();
    }
    atexit(SDL_Quit);
    inited = true;
  }
  if(init_systems & system) return;
  if(SDL_InitSubSystem(system)) throw (const char*)SDL_GetError();
  init_systems |= system;
}

void SDLMan::QuitSubsystem(Uint32 system) {
  if(init_failed) throw (const char*)"Initialization already failed once";
  if(!inited) throw (const char*)"Not initialized in the first place";
  if(init_systems & system) {
    init_systems &= ~system;
    SDL_QuitSubSystem(system);
  }
}
