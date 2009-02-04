local cursor
function uranium.cursor()
   return cursor,1,2
end

local pngload = assert(SC.Construct("PNGLoader"))
cursor = assert(pngload:Load(P"cursor.png"))
