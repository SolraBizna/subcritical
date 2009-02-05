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

local button = {}
uranium.button = button

local diagon_tl,diagon_br,diagon_bl,diagon_tr
do
   --local pngload = assert(SC.Construct("PNGLoader"))
   --local diagon = assert(pngload:Load(P"diagon.png"))
   local function diagon_render(x,y)
      x = (4 - x)
      y = (4 - y)
      local d = math.sqrt(x*x+y*y)
      return 0,d-4,0
   end
   local diagon = SCUtil.RenderPreCompressed(diagon_render, 4, 4)
   diagon_tl=SCUtil.MakeFrisketDirectly(diagon)
   diagon_br=SCUtil.MakeFrisketDirectly(SCUtil.Flip(diagon))
   diagon_bl=SCUtil.MakeFrisketDirectly(SCUtil.RotateLeft(diagon))
   diagon_tr=SCUtil.MakeFrisketDirectly(SCUtil.RotateRight(diagon))
end

function button:RenderSelf(active)
   assert(self.x and self.y and self.w and self.h, "malformed button")
   local sx,sy = self._x,self._y
   if (self.active and self.focus and not self.disabled) then
      screen:SetPrimitiveColor(3/4, 3/4, 3/4)
   else
      screen:SetPrimitiveColor(0, 0, 0)
   end
   if(self.hotkeys["return"]) then
      screen:DrawRect(sx+2, sy+2, self.w-4, self.h-4)
   else
      screen:DrawRect(sx+1, sy+1, self.w-2, self.h-2)
   end
   if(not self.disabled) then
      screen:SetPrimitiveColor(1, 1, 1)
   else
      screen:SetPrimitiveColor(1/4, 1/4, 1/4)
   end
   if(self.hotkeys["return"]) then
      screen:DrawBox(sx, sy, self.w, self.h, 2)
      screen:BlitFrisket(diagon_tl,sx+2,sy+2)
      screen:BlitFrisket(diagon_br,sx+self.w-6,sy+self.h-6)
      screen:BlitFrisket(diagon_bl,sx+2,sy+self.h-6)
      screen:BlitFrisket(diagon_tr,sx+self.w-6,sy+2)
   else
      screen:DrawBox(sx, sy, self.w, self.h, 1)
      screen:BlitFrisket(diagon_tl,sx+1,sy+1)
      screen:BlitFrisket(diagon_br,sx+self.w-5,sy+self.h-5)
      screen:BlitFrisket(diagon_bl,sx+1,sy+self.h-5)
      screen:BlitFrisket(diagon_tr,sx+self.w-5,sy+1)
   end
   if(self.text) then
      if(not self.font) then self.font = uranium.font end
      if(not self.frisket) then
	 self.frisket = assert(self.font:RenderText(self.text))
      end
      if(not self.disabled) then
	 if(self.active and self.focus) then
	    screen:SetPrimitiveColor(0, 0, 0)
	 elseif (self.focus) then
	    screen:SetPrimitiveColor(1, 1, 1)
	 else
	    screen:SetPrimitiveColor(3/4, 3/4, 3/4)
	 end
      end
      local fw,fh = self.frisket:GetSize()
      local ty
      if(fh > self.h-6) then
	 ty = sy+3
      else
	 ty = sy + math.floor((self.h-fh)/2)
      end
      if(fw > self.w-6) then
	 screen:BlitFrisket(self.frisket, 0, 0, self.w-6, self.h-6, sx+3, ty)
      else
	 screen:BlitFrisket(self.frisket, 0, 0, fw, self.h-2, sx+math.floor((self.w-fw)/2), ty)
      end
   end
end

function button:OnMouseDown()
   if(self.disabled) then return end
   self.active = true
   scui.MarkDirty(self)
end

function button:OnMouseUp()
   if(self.disabled) then return end
   self.active = false
   scui.MarkDirty(self)
   if(self.focus) then
      return self:action()
   end
end

function button:OnMouseEnter()
   if(self.disabled) then return end
   self.focus = true
   scui.MarkDirty(self)
end

function button:OnMouseLeave()
   if(self.disabled) then return end
   self.focus = false
   scui.MarkDirty(self)
end

function button:OnHotKey()
   if(self.disabled) then return end
   local a,f = self.active,self.focus
   self.active,self.focus=true,true
   scui.RenderWidget(self)
   screen:Update(self._x, self._y, self.w, self.h)
   SCUtil.Sleep(0.1)
   self.active,self.focus=a,f
   scui.RenderWidget(self)
   screen:Update(self._x, self._y, self.w, self.h)
   return self:action()
end

function button:action()
   return self.tag
end

function button:Disable()
   if(not self.disabled) then
      self.disabled = true
      scui.MarkDirty(self)
   end
end

function button:Enable()
   if(self.disabled) then
      self.disabled = false
      scui.MarkDirty(self)
   end
end
