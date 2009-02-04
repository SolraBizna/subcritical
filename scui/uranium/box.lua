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

