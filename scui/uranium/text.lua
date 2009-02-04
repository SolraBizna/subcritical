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
