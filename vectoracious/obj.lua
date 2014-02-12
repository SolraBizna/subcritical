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

local vgl = vgl
function vgl.load_mtl(mtlpath)
   local materials = {}
   local f = assert(io.open(mtlpath, "r"))
   local curmtl
   for l in f:lines() do
      l = l:gsub("#.*$","")
      if l:sub(1,7) == "newmtl " then
	 local mtlname = assert(l:match("^newmtl[ \t]+(.*)$"), "Invalid .mtl file, or at least I can't understand it")
	 assert(not materials[mtlname], "A material with the same name was specified more than once")
	 materials[mtlname] = {name=mtlname,0.8,0.8,0.8,1}
	 curmtl = materials[mtlname]
      elseif l:sub(1,3) == "Kd " then
	 assert(curmtl, "Invalid .mtl file, or at least I can't understand it")
	 local r,g,b = l:match("^Kd[ \t]+([-+0-9.e]+)[ \t]+([-+0-9.e]+)[ \t]+([-+0-9.e]+)")
	 assert(r, "Invalid diffuse color, or at least I can't understand it")
	 r = assert(tonumber(r),"Invalid red value")
	 g = assert(tonumber(g),"Invalid green value")
	 b = assert(tonumber(b),"Invalid blue value")
	 curmtl[1],curmtl[2],curmtl[3] = r,g,b
      elseif l:sub(1,2) == "d " then
	 assert(curmtl, "Invalid .mtl file, or at least I can't understand it")
	 local a = assert(l:match("^d[ \t]+([-+0-9.e]+)"), "Invalid opacity, or at least I can't understand it")
	 a = assert(tonumber(a),"Invalid opacity")
	 curmtl[4] = a
      elseif l:sub(1,2) == "Tr " then
	 assert(curmtl, "Invalid .mtl file, or at least I can't understand it")
	 local a = assert(l:match("^Tr[ \t]+([-+0-9.e]+)"), "Invalid opacity, or at least I can't understand it")
	 a = assert(tonumber(a),"Invalid opacity")
	 curmtl[4] = a
      end
   end
   f:close()
   return materials
end

local obj2d_instance = {}
function vgl.load_obj_2d(objpath, mtlpath)
   local materials
   if mtlpath then
      materials = vgl.load_mtl(mtlpath)
   else
      materials = {}
   end
   if not materials.__defmtl__ then
      materials.__defmtl__ = {name="__defmtl__",0.8,0.8,0.8,1}
   end
   local view = vgl.instantiate(obj2d_instance)
   local vertices = {}
   local faces = {}
   local curmtl = materials.__defmaterial__
   local f = assert(io.open(objpath, "r"))
   for l in f:lines() do
      if(l:sub(1,2) == "v ") then
	 local x,y = l:match("^v[ \t]+([-+0-9.e]+)[ \t]+([-+0-9.e]+)")
	 assert(x, "Unintelligible vertex")
	 x = assert(tonumber(x),"Unintelligible X")
	 y = assert(tonumber(y),"Unintelligible Y")
	 vertices[#vertices+1] = {x,y}
      elseif (l:sub(1,7) == "usemtl ") then
	 local mtl = assert(l:match("^usemtl[ \t]+(.*)$"), "Unintelligible material")
	 curmtl = materials[mtl] or materials.__defmaterial__
      elseif (l:sub(1,2) == "f ") then
	 local i = {}
	 for v in l:gmatch("([0-9]+)[^ \t]*") do
	    i[#i+1] = tonumber(v)
	    assert(vertices[tonumber(v)], "A face referenced a vertex that didn't exist")
	 end
	 assert(#i >= 3, "Face with fewer than three vertices (try using the line-specific loading stuff")
	 for n=2,#i-1,1 do
	    faces[#faces+1] = {mtl=curmtl,i[1],i[n],i[n+1]}
	 end
      end
   end
   f:close()
   -- Check winding / area; delete inappropriate triangles
   for i=#faces,1,-1 do
      local face = faces[i]
      local a,b,c = vertices[face[1]],vertices[face[2]],vertices[face[3]]
      if (a[1] == b[1] and a[2] == b[2]) or
	 (a[1] == c[1] and a[2] == c[2]) or
	 (b[1] == c[1] and b[2] == c[2]) or
         (a[1]*b[2] - b[1]*a[2]) + (b[1]*c[2] - c[1]*b[2]) + (c[1]*a[2] - a[1]*c[2]) <= (1/1048576) then
	 table.remove(faces, i)
      end
   end
   -- Check for unused vertices
   local vref,vmap = {}
   for i=1,#vertices do
      vref[i] = 0
   end
   for i=1,#faces do
      local face = faces[i]
      vref[face[1]] = vref[face[1]] + 1
      vref[face[2]] = vref[face[2]] + 1
      vref[face[3]] = vref[face[3]] + 1
   end
   local vmap = {}
   local vt = {}
   for i=1,#vertices do
      if vref[i] > 0 then
	 vmap[i] = #vt
	 vt[#vt+1] = SC.Construct("Vec2", unpack(vertices[i]))
      end
   end
   view.vertices = SC.Construct("VectorArray", vt)
   curmtl = nil
   local passes = {}
   view.passes = passes
   local pass
   for i=1,#faces do
      local face = faces[i]
      if face.mtl ~= curmtl then
	 curmtl = face.mtl
	 pass = {i={},mtl=face.mtl}
	 passes[#passes+1] = pass
      end
      pass.i[#pass.i+1] = vmap[face[1]]
      pass.i[#pass.i+1] = vmap[face[2]]
      pass.i[#pass.i+1] = vmap[face[3]]
   end
   for i=1,#passes do
      pass = passes[i]
      pass.i = assert(SCUtil.CompileIndices(pass.i))
   end
   return view
end

function obj2d_instance:render(vglc, obj)
   local face = obj.face or 0
   local c,s = math.cos(face),math.sin(face)
   local x,y
   if obj.pos then x,y = obj.pos() else x,y = 0,0 end
   if obj.scale then c,s = c*obj.scale,s*obj.scale end
   local matrix = SC.Construct("Mat2x4", c, s, -s, c, 0, 0, x, y)
   if vglc.matrix then
      matrix = vglc.matrix * matrix
   end
   local ca = matrix:MultiplyAndCompile(self.vertices, vglc.hcw, vglc.hch)
   local canvas = vglc.canvas
   local hcw,hch = vglc.hcw,vglc.hch
   for n=1,#self.passes do
      local pass = self.passes[n]
      local r,g,b,a
      if obj.materials and obj.materials[pass.mtl.name] then
	 r,g,b,a = unpack(obj.materials[pass.mtl.name])
      else
	 r,g,b,a = unpack(pass.mtl)
      end
      if obj.alpha then
	 a = obj.alpha
      end
      canvas:SetPrimitiveColor(r,g,b,a)
      canvas:DrawTriangles(ca, pass.i)
   end
end

function obj2d_instance:interpolate(vglc, a, b, i)
   local out = {materials=a.materials,view=self}
   if a.alpha or b.alpha then
      out.alpha = vgl.lerp(a.alpha or 1, b.alpha or 1, i)
   end
   if a.face or b.face then
      out.face = vgl.lerp_angle(a.face or 0, b.face or 0, i)
   end
   if a.scale or b.scale then
      out.scale = vgl.lerp(a.scale or 1, b.scale or 1, i)
   end
   if a.pos or b.pos then
      out.pos = vgl.lerp(a.pos or SC.Construct("Vec2", 0, 0),
			 a.pos or SC.Construct("Vec2", 0, 0), i)
   end
   return out
end
