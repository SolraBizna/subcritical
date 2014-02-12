// -*- c++ -*-
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

ret->C2(ROW,COL) = CPP(C3(A_,ROW,x)) * CPP(C3(B_,x,COL))
  + CPP(C3(A_,ROW,y)) * CPP(C3(B_,y,COL))
#if MAX_RC >= 3
  + CPP(C3(A_,ROW,z)) * CPP(C3(B_,z,COL))
#endif
#if MAX_RC >= 4
  + CPP(C3(A_,ROW,w)) * CPP(C3(B_,w,COL))
#endif
  ;
