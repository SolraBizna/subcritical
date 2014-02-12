--[[
  This source file is part of the SubCritical distribution.
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
]]

local uranium = uranium
uranium.box = {}

function uranium.box:RenderSelf(window)
   assert(self.w and self.h, "box with no width/height?")
   screen:SetPrimitiveColor(1, 1, 1)
   uranium.DrawBox(self._x, self._y, self.w, self.h)
   if(self.filled) then
      screen:SetPrimitiveColor(0, 0, 0)
      screen:DrawRect(self._x+1, self._y+1, self.w-2, self.h-2)
   end
end

uranium.hrule = {}

function uranium.hrule:RenderSelf(window)
   assert(self.w, "hrule with no width?")
   self.h = 1
   screen:SetPrimitiveColor(1, 1, 1)
   screen:DrawRect(self._x, self._y, self.w, 1)
end

uranium.vrule = {}

function uranium.vrule:RenderSelf(window)
   assert(self.h, "vrule with no height?")
   self.w = 1
   screen:SetPrimitiveColor(1, 1, 1)
   screen:DrawRect(self._x, self._y, 1, self.h)
end

