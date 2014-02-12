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
local scui = require "scui"
local field = {}
uranium.field = field

function field:UpdateFrisket()
   if(not self.font) then self.font = uranium.font end
   if(not self.text) then self.text = "" end
   if(self.text == "") then self.frisket = nil
   else
      self.frisket,self.ipp = assert(self.font:RenderText(self.text))
      self.ipp = math.floor(self.ipp)
   end
end

function field:RenderSelf()
   local sx,sy = self._x,self._y
   if(not self.frisket) then self:UpdateFrisket() end
   screen:SetPrimitiveColor(0, 0, 0)
   screen:DrawRect(sx+1, sy+1, self.w-2, self.h-2)
   if(self.disabled) then
      screen:SetPrimitiveColor(1/4, 1/4, 1/4)
   else
      screen:SetPrimitiveColor(1, 1, 1)
   end
   screen:DrawBox(sx, sy, self.w, self.h)
   local ipp
   if(self.frisket) then
      if(self.ipp > self.w - 8) then
	 local off = self.ipp - (self.w - 6)
	 screen:BlitFrisket(self.frisket, off, 0, self.w-6, self.h-3, sx+1, sy+3)
	 ipp = sx+self.w-4
      else
	 screen:BlitFrisket(self.frisket, 0, 0, self.w-8, self.h-3, sx+3, sy+3)
	 ipp = sx+math.floor(self.ipp)+4
      end
   else ipp = sx+3
   end
   if(self.focus and not self.disabled) then
      screen:DrawRect(ipp, sy+3, 1, self.h-6)
   end
end

function field:OnMouseEnter()
   self.focus = true
   scui.MarkDirty(self)
end

function field:OnMouseLeave()
   self.focus = false
   scui.MarkDirty(self)
end

function field:OnText(char)
   if(self.disabled) then return end
   self.frisket = nil
   self.text = self.text or ""
   if(char == "\8" or char == "\127") then -- control-H, backspace
      self.text = self.text:sub(1,-2)
   elseif (char == "\21") then -- control-U
      self.text = ""
   else
      self.text = self.text .. char
   end
   scui.MarkDirty(self)
end

function field:Disable()
   if(not self.disabled) then
      self.disabled = true
      scui.MarkDirty(self)
   end
end

function field:Enable()
   if(self.disabled) then
      self.disabled = false
      scui.MarkDirty(self)
   end
end
