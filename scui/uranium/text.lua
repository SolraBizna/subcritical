--[[
  This source file is part of the SubCritical distribution.
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
]]

uranium.text = {}

function uranium.text:SetupText()
   local biggest_w = 0
   local n = 1
   local start = 1
   local y = 0
   if(not self.font) then self.font = uranium.font end
   local text,font = self.text,self.font
   local skip = font:GetLineHeight()
   self.skip = skip
   self.friskets = {}
   repeat
      local stop,nustart = font:BreakLine(text, self.w, start)
      if(stop) then
	 if(stop < start) then
	    self.friskets[n] = false
	 else
	    self.friskets[n] = font:RenderText(text, start, stop, self.width, 0, y)
	    local w,h = self.friskets[n]:GetSize()
	    if(w > biggest_w) then biggest_w = w end
	 end
	 n = n + 1
	 y = y + skip
	 start = nustart
      end
   until not start
   if(not self.w) then self.w = biggest_w end
   if(not self.h) then self.h = math.ceil(y) end
end

function uranium.text:RenderSelf()
   local x,y = self._x,self._y
   assert(self.text, "text object with no text?!")
   if(not self.friskets) then
      self:SetupText()
   end
   local skip = self.skip
   local remh = self.h
   screen:SetPrimitiveColor(0, 0, 0)
   screen:DrawRect(x, y, self.w, self.h)
   screen:SetPrimitiveColor(1, 1, 1)
   for i,v in ipairs(self.friskets) do
      if(remh <= 0) then return end
      if(v) then
	 screen:BlitFrisket(v, 0, 0, self.w, math.ceil(remh), x, math.floor(y))
      end
      y = y + skip
      remh = remh - skip
   end
end
