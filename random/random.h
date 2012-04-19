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

#ifndef __SUBCRITICAL_RANDOM_H
#define __SUBCRITICAL_RANDOM_H

#include "subcritical/core.h"

namespace SubCritical {
  class RNG : public Object {
  public:
    PROTOCOL_PROTOTYPE();
    virtual uint32_t Random32() throw();
    virtual double RandomD() throw();
    int Lua_Random(lua_State* L) throw();
  };
}

#endif

