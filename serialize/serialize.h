// -*- c++ -*-
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
#ifndef _SUBCRITICAL_SERIALIZE_H
#define _SUBCRITICAL_SERIALIZE_H

#include "subcritical/core.h"

namespace SubCritical {
  LOCAL extern bool float_little_endian;
  LOCAL extern void build_crc_table();
  enum CelduinAtom {
    Nil = 0x00,
    False = 0x01,
    True = 0x02,
    EndTable = 0x03,
    BigMask = 0xE0,
    String = 0x80,
    Table = 0xA0,
    Number = 0xC0,
  };
}

#endif
