#!/usr/bin/env lua
-- -*- lua -*-
-- I had the misfortune of reading this file after writing it, and all I can
-- say is that it's still better than autotools.

--[[
  This source file is part of the SubCritical kernel.
  Copyright (C) 2008-2012 Solra Bizna.

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

-- simple os detection
local onwindows = not not (os.getenv"SYSTEMROOT" or os.getenv"SCBUILD_WINDOWS")
local oncygwin = not not (os.getenv"PATH" or ""):match"cygdrive"
local onposix = not not (os.getenv"HOME" or oncygwin)
local onpurewindows = onwindows and not onposix

-- use sh to run posix commands under a lua built for pure windows
if onwindows and onposix then
   local oexecute = os.execute

   function os.execute (cmd)
      return oexecute(("sh -c %q"):format("exec "..cmd))
   end
end

-- check version
-- we're going to have to do an identical check in subcritical.lua
do
   local major, minor = string.match(_VERSION or "", "([0-9]+)%.([0-9]+)")
   major = tonumber(major)
   minor = tonumber(minor)
   if not major or not minor then
      io.stderr:write("WARNING:\nCan't identify the running Lua version. SubCritical requires Lua 5.2.\n")
   elseif major < 5 or major == 5 and minor < 2 then
      io.stderr:write("Your Lua interpreter is too old. Lua 5.2 is required, but you have ".._VERSION..".\nPlease install Lua 5.2 before attempting to build SubCritical.\n")
      os.exit(1)
   elseif major > 5 or minor > 2 then
      io.stderr:write("WARNING:\nYour Lua interpreter may be too new. SubCritical is designed for Lua 5.2,\nbut you have ".._VERSION..". There may be incompatibilities.\n")
   end
end

if(arg and #arg >= 1 and arg[1] == "clean") then
   -- Special case!
   local clean_command = "rm -f core *~ \\#*\\# *.o *.so *.scc *.dylib *.bundle *.dll *.a *.d"
   print(clean_command)
   local status = os.execute(clean_command)
   if(status ~= true or #arg == 1) then os.exit(1) end
end

-- Set these in the environment of an automatic build script. You'd be
-- responsible for setting up a .scbuild with all needed information.
local noprompt = not not os.getenv("SUBCRITICAL_AUTOMATIC_BUILD")
local defpath
local home

if(onpurewindows or os.getenv("SCBUILD_WINDOWS")) then
   home = os.getenv("USERPROFILE") or "C:\\Documents and Settings\\User"
   defpath = os.getenv("USERPROFILE").."\\scbuild.conf"
else
   home = os.getenv("HOME") or "/home"
   defpath = ((os.getenv("HOME") or "") .. "/.scbuild")
end

local confpath = os.getenv("SUBCRITICAL_BUILD_CONFIG") or defpath

local source,err = loadfile("build.scb")
if(not source) then
   print("Fatal: No build.scb found in working directory, or syntax error.")
   os.exit(1)
end

local confcheck = "(for "..(_VERSION or "unknown Lua version")..")"

local config = {}
do
   local f = io.open(confpath, "r")
   if(f) then
      if f:read() ~= confcheck then
         print("The last time scbuild was run, it was for a different version of Lua.\nDiscarding all config information.")
      else
         for l in f:lines() do
            local k,v = l:match("^([^=]+)=(.*)$")
            if(k) then
               if(v=="__true") then config[k] = true
               elseif (v=="__false") then config[k] = false
               else config[k] = v
               end
            end
         end
      end
      f:close()
   end
end

function config_question(cachename, prompt, type, ...)
   if(config[cachename] ~= nil) then return config[cachename]
   elseif (noprompt) then
      error("we were asked for "..cachename..", which I don't know, but we are forbidden to ask the user", 1)
   elseif (not prompt or not type) then
      error("we were asked for "..cachename..", which I don't know, but didn't give me enough information (does it depend on another package?)", 1)
   else
      if(type == "freeform") then
	 local default = ...
	 print(prompt)
	 if(default) then print(("(default: %s)"):format(default)) end
	 config[cachename] = assert(io.read("*l"))
	 if(default and config[cachename] == "") then config[cachename] = default end
	 return config[cachename]
      elseif (type == "list") then
	 local arg = {...}
	 io.write(prompt.."\nEnter a number. Choices are:\n")
	 for i=1,#arg,2 do
	    io.write(("%i. %s\n"):format(i/2+0.5,arg[i]))
	 end
	 local choice = tonumber(assert(io.read("*l")))
	 if(not choice or not arg[choice*2]) then
	    if arg[1]:match("default") then
               config[cachename] = arg[2]
	       return arg[2]
	    else
	       print("Fatal: That's not a choice. Try again.")
	       os.exit(1)
	    end
	 end
	 config[cachename] = arg[choice*2]
	 return config[cachename]
      elseif (type == "bool") then
         local default = ...
         if default then
            io.write(prompt.." (YES/no) ")
         else
            io.write(prompt.." (yes/NO) ")
         end
	 io.flush()
	 local response = io.read("*l"):lower()
         if default then
            config[cachename] = not ((tonumber(response) and tonumber(response) == 0) or response:sub(1,1) == "n" or response:sub(1,1) == "f")
         else
            config[cachename] = (tonumber(response) and tonumber(response) ~= 0) or response:sub(1,1) == "y" or response:sub(1,1) == "t"
         end
	 return config[cachename]
      else
	 error("we were asked for "..cachename..", which I don't know, and it was explained it to me in a weird way.", 1)
      end
   end
end

--print("Detecting Lua flags...")
local lua_cflags = ""
if os.execute("pkg-config lua5.2 --exists") then
   lua_cflags="`pkg-config lua5.2 --cflags`"
else
   local searchpath = {
      "/opt/local",
      "/opt",
      "/usr/local",
      "/usr",
   }
   local subpath = {
      "/",
      "/lua5.2/",
      "/lua52/"
   }
   if os.getenv("HOME") then
      table.insert(searchpath,1,os.getenv("HOME"))
   end
   local exists
   if onwindows and onposix then
      function exists(path)
         return os.execute("ls " .. path .. " &> blah")
      end
   else
      function exists(path)
         local f = io.open(path,"r")
         if f then
            f:close()
            return true
         else
            return false
         end
      end
   end
   for n=1,#searchpath do
      for m=1,#subpath do
         local dir = searchpath[n].."/include"..subpath[m]
         if exists(dir.."lua.h") then
            lua_cflags = "-I\""..dir.."\""
            break
         end
      end
      if lua_cflags ~= "" then break end
   end
end

local osc
local cxx
local cxx_incflags
local ld
local ld_libflags
local soext
osc = config_question("OS/COMPILER",
		      "Select your OS/compiler combination from the list.",
		      "list",
		      "Generic UNIX with GCC (Linux, BSD)", "linux",
		      "Cygwin", "cygwin",
		      "MinGW on Cygwin", "cygwin_mingw",
		      "MinGW on Windows", "mingw",
		      "Darwin (Mac OS X)", "darwin",
		      "Other (manual)", "other")
if(osc == "other") then
   cxx = config_question("OTHER_CXX",
			 "Enter the full command line to compile C++ code.\nThe compiler is expected to understand -c and -o.",
			 "freeform")
   ld = config_question("OTHER_LD",
			"Enter the full command line to link C++ code into a shared library.\nThe linker is expected to understand -o.",
			"freeform")
   soext = config_question("OTHER_SOEXT",
			   "Enter the extension (including the period, if any) for shared libraries.",
			   "freeform")
else
   local gpp = os.getenv("CXX") or "g++"
   local gld = os.getenv("LD") or os.getenv("CXX") or "g++"
   local platforms = {
      linux={cxx=gpp.." -pthread -Wall -Wno-pmf-conversions -fPIC -O2 "..lua_cflags.." -c", ld=gld.." -pthread -fPIC -O -shared", soext=".so"},
      --cygwin={cxx=gpp.." -Wall -Wno-pmf-conversions -O2 "..lua_cflags.." -c", ld=gld.." -pthread -O -shared", soext=".dll"},
      --mingw={cxx=gpp.." -Wall -Wno-pmf-conversions -DHAVE_WINDOWS -O2 "..lua_cflags.." -c", ld=gld.." -O -shared", soext=".dll"},
      cygwin={cxx=gpp.." -pthread -g -Wall -Wno-pmf-conversions -O2 "..lua_cflags.." -c", ld=gld.." -pthread -O -shared", ld_libflags="-llua", soext=".scc"},
      cygwin_mingw={cxx=gpp.." -pthread -mno-cygwin -Wall -Wno-pmf-conversions -DHAVE_WINDOWS -O2 "..lua_cflags.." -c", ld=gld.." -pthread -mno-cygwin -O -shared", ld_libflags="-llua", soext=".scc"},
      mingw={cxx=gpp.." -pthread -Wall -Wno-pmf-conversions -DHAVE_WINDOWS -O3 "..lua_cflags.." -c", ld=gld.." -pthread -O -L/usr/local/lib -shared -static-libgcc", ld_libflags="-llua", soext=".scc"},
      darwin={cxx=gpp.." -pthread -Wall -Wno-pmf-conversions -O2 "..lua_cflags.." -fPIC -fno-common -c", ld="MACOSX_DEPLOYMENT_TARGET=\"10.3\" "..gld.." -pthread -bundle -undefined dynamic_lookup -Wl,-bind_at_load", soext=".scc"},
   }
   local platform = platforms[osc]
   assert(platform)
   cxx = platform.cxx
   cxx_incflags = config.CXX_INCLUDE_FLAGS
   ld = platform.ld
   do local t = {}
      table.insert(t, ld_libflags)
      table.insert(t, config.LD_LIBRARY_FLAGS)
      table.insert(t, platform.ld_libflags)
      ld_libflags = table.concat(t, " ")
   end
   soext = platform.soext
end

local install_cmd
if(onpurewindows) then install_cmd = "copy" else install_cmd = "install" end
install_cmd = os.getenv("INSTALL_CMD") or install_cmd

if(cxx:match("g%+%+")) then
   if(config_question("VISIBILITY_HIDDEN", "Does your GCC support -fvisibility=hidden? (GCC 4.0 and above)", "bool")) then
      cxx = cxx .. " -fvisibility=hidden -fvisibility-inlines-hidden -DHAVE_GCC_VISIBILITY"
   end
   if(config_question("WERROR", "Do you wish to use -Werror? (Unless you know you do, you don't.)", "bool")) then
      cxx = cxx:gsub("%-Wall", "-Wall -Werror")
   end
end

local install_path

if(onwindows and not oncygwin) then
   local subcritical_path = "\\SubCritical\\"
   if onposix then
      subcritical_path = subcritical_path:gsub("\\", "/")
   end
   install_path = config_question("INSTALL_PATH",
				     "Where do you want SubCritical to live? You can always move it later.",
				     "list",
				     home..subcritical_path.." (default)", home..subcritical_path,
				     "C:"..subcritical_path, "C:"..subcritical_path,
				     "Other", "")
else
   install_path = config_question("INSTALL_PATH",
				     "Where do you want SubCritical to live? You can always move it later.",
				     "list",
				     "/usr/local/subcritical (default)", "/usr/local/subcritical/",
				     "/usr/subcritical", "/usr/subcritical/",
				     "Other", "")
end

if(install_path == "") then
   install_path = config_question("REAL_INSTALL_PATH", "Enter the full path to where you want SubCritical to live.", "freeform", home.."/.subcritical/")
end
if(onpurewindows) then
   function sanitize_path(path)
      return (path.."\\"):gsub("\\\\+", "\\")
   end
else
   function sanitize_path(path)
      return (path.."/"):gsub("//+", "/")
   end
end
install_path = sanitize_path(install_path)

if(install_path:match("[ \"'!;]")) then
   print("ERROR: Please pick an install path with as few special characters as possible.")
   os.exit(1)
end

local include_path
if(onpurewindows) then
   include_path = install_path.."include\\"
else
   include_path = install_path.."include/"
end
include_path = sanitize_path(include_path)

local install_lib,install_c,install_lua
if(onpurewindows) then
   install_lib = install_path.."lib\\"
else
   install_lib = install_path.."lib/"
end
install_lib = install_lib:gsub("//+", "/")

local cpath
for entry in package.cpath:gmatch("[^;]+") do
   if(entry:sub(1,1) ~= "." and entry:sub(1,1) ~= "@") then
      cpath = entry:gsub("%?.+$","")
      break
   end
end
install_c = config_question("INSTALL_C",
			    "Enter the path where we should install sc_helper.so so that Lua can find it:\n(note that you may need to set LUA_CPATH in the environment if you change this\nto something your local Lua interpreter won't expect)",
			    "freeform",
			    cpath) -- cpath can be nil, making prompt silly
install_c = sanitize_path(install_c)
local lpath
for entry in package.path:gmatch("[^;]+") do
   if(entry:sub(1,1) ~= "." and entry:sub(1,1) ~= "@") then
      lpath = entry:gsub("%?.+$","")
      break
   end
end
install_lua = config_question("INSTALL_LUA",
			      "Enter the path where we should install subcritical.lua so that Lua can find it:\n(note that you may need to set LUA_PATH in the environment if you change this\nto something your local Lua interpreter won't expect)",
			      "freeform",
			      lpath)
install_lua = sanitize_path(install_lua)

cxx = cxx .. " -I"..include_path
if cxx_incflags then cxx = cxx .. " " .. cxx_incflags end
cxx = cxx .. " -DSO_EXTENSION=\"\\\""..soext.."\\\"\""

-- Try our best to find subcritical_helper
if(not onwindows) then
   package.cpath = "./?"..soext..";../?"..soext
   if(install_c) then
      package.cpath = package.cpath..";"..install_c.."/?"..soext
   end
else
   package.cpath = ".\\?"..soext..";../?"..soext
   if(install_c) then
      package.cpath = package.cpath..";"..install_c.."\\?"..soext
   end
end

if(os.getenv("CXXFLAGS") or os.getenv("CFLAGS")) then
   cxx = cxx .. " " .. (os.getenv("CXXFLAGS") or os.getenv("CFLAGS"))
end
if(os.getenv("LDFLAGS")) then
   ld = ld .. " " .. os.getenv("LDFLAGS")
end

local hack_flags
if(os.getenv("STUPID_LINKER_HACK_FLAGS")) then hack_flags = " " .. os.getenv("STUPID_LINKER_HACK_FLAGS") else hack_flags = "" end
if(osc == "mingw") then hack_flags = " /mingw/lib/lua51.dll " .. hack_flags end

function append_cxxflags(flags)
   cxx = cxx .. " " .. flags
end
append_cflags = append_cxxflags

function append_ldflags(flags)
   ld = ld .. " " .. flags
end

local success,err = xpcall(source, debug.traceback)
if(not success) then
   print("Lua error in build.scb:")
   print(err)
   os.exit(1)
end

do
   local f = io.open(confpath, "w")
   if(f) then
      f:write(confcheck.."\n")
      local t = {}
      for k,v in pairs(config) do
	 if(type(v) == "string") then
        table.insert(t, ("%s=%s\n"):format(k, v))
	 else
        table.insert(t, ("%s=__%s\n"):format(k, tostring(v)))
	 end
      end
      table.sort(t, function (a, b) return a < b end)
      for i = 1, #t do f:write(t[i]) end
      f:close()
      --print("Remember, you can reconfigure the build by removing "..confpath)
   else
      print("WARNING: Couldn't write "..confpath)
   end
end

if(not targets) then
   print("ERROR: build.scb did not provide any targets!")
   os.exit(1)
end

local function source_to_object(name)
   if(name:match("%.([^.]+)$")) then return name:gsub("%.([^.]+)$", ".o")
   else return name..".o" end
end

local function mkdir(dir)
   dir = dir:gsub("[\\/]+$","")
   print("(checking/making directory \""..dir.."\")")
   helper.ckdir(dir)
end

local real_targets,virtual_targets,fake_targets = {},{},{}
-- clean doesn't need to read the package data, so it's handled at the top
function fake_targets.install()
   assert(real_targets.all)
   build("all", false)
   local function pe(v)
      print(v)
      local success,error = os.execute(v)
      if not success then
	 print("*** Install command ended with error "..error)
	 os.exit(1)
      end
   end
   assert(install, "build.scb did not provide an install table")
   mkdir(install_path)
   if(install.headers) then
      mkdir(include_path)
      mkdir(include_path.."subcritical")
      for i,header in pairs(install.headers) do
	 print("(installing and converting "..header..")")
	 local i = assert(io.open(header, "r"))
	 local install_path
	 if(onpurewindows) then
            install_path = include_path.."subcritical\\"..header
         else
            install_path = include_path.."subcritical/"..header
         end
	 local o,e = io.open(install_path, "w")
	 if(not o) then
	    print(e)
	    return false
	 end
	 for l in i:lines() do
	    if(l:sub(1,1) ~= "#") then l = l:gsub("EXPORT", "IMPORT") end
	    o:write(l.."\n")
	 end
	 i:close()
	 o:close()
      end
   end
   if(install.packages) then
      mkdir(install_lib)
      if(type(install.packages) == "string") then
	 print("Ahem! install.packages in your build.scb file needs to be a table. Pretending\nI didn't see that...")
	 install.packages = {install.packages}
      end
      for i,package in pairs(install.packages) do
	 pe(install_cmd.." "..package..soext.." \""..install_lib.."\"")
	 pe(install_cmd.." "..package..".scp \""..install_lib.."\"")
      end
   end
   if(install.lpackages) then
      mkdir(install_lua)
      for i,package in pairs(install.lpackages) do
	 pe(install_cmd.." "..package.." \""..install_lua.."\"")
      end
   end
   if(install.lcpackages) then
      mkdir(install_c)
      for i,package in pairs(install.lcpackages) do
	 pe(install_cmd.." "..package..soext.." \""..install_c.."\"")
      end
   end
   if(install.utilities) then
      mkdir(install_lib)
      for i,utility in pairs(install.utilities) do
	 pe(install_cmd.." "..utility.." \""..install_lib.."\"")
      end
   end
   print("Installed.")
   return true
end

local hack_flags = os.getenv("STUPID_LINKER_HACK_FLAGS")
for soname,target in pairs(targets) do
   local sofile = soname..soext
   virtual_targets[soname] = sofile
   local full_ld = ld
   full_ld = full_ld .. " -o " .. sofile
   local real_target = {deps={}}
   real_targets[sofile] = real_target
   for i,source in ipairs(target) do
      local object = source_to_object(source)
      full_ld = full_ld .. " " .. object
      real_target.deps[#real_target.deps+1] = object
   end
   if(target.deps and osc ~= "darwin") then
      for _,dep in ipairs(target.deps) do
	 full_ld = full_ld .. " \"" .. install_lib .. dep .. soext .. "\""
      end
   end
   if(target.libflags) then full_ld = full_ld .. " " .. target.libflags end
   if(ld_libflags) then full_ld = full_ld .. " " .. ld_libflags end
   if(hack_flags) then full_ld = full_ld .. " " .. hack_flags end
   real_target.commands = {full_ld}
   for i,source in ipairs(target) do
      local object = source_to_object(source)
      real_targets[object] = {deps={source}, commands={cxx.." -c -o "..object.." "..source}}
      real_targets[source] = {deps={}, built=true}
   end
   real_targets[sofile] = real_target
end
do
   local all = {deps={}}
   for vtarget in pairs(virtual_targets) do
      all.deps[#all.deps+1] = vtarget
   end
   real_targets["all"] = all
end

if(os.getenv("SCBUILD_DEBUG")) then
   for k,v in pairs(virtual_targets) do
      print("VIRTUAL("..k..") -> "..v)
   end
   for k,target in pairs(real_targets) do
      print("REAL("..k..") <- "..table.concat(target.deps, ", "))
      if(target.commands) then
	 for i,command in ipairs(target.commands) do
	    print("\t"..command)
	 end
      end
   end
end

local targets_built,targets_total
local erase_length
function build(target, fake)
   if(virtual_targets[target]) then return build(virtual_targets[target], fake) end
   if(fake_targets[target]) then
      if(fake) then return 0
      else return fake_targets[target]() end
   end
   local target = assert(real_targets[target])
   if(target.built) then
      if(fake) then return 0
      else return true end
   else
      local ret
      if(fake) then
	 ret = 0
	 for i,v in ipairs(target.deps) do
	    ret = ret + build(v, fake)
	 end
      else
	 for i,v in ipairs(target.deps) do
	    if(not build(v, fake)) then return false end
	 end
      end
      if(not target.commands) then
	 if(fake) then return ret
	 else return true end
      elseif (fake) then
	 if(not target.faked) then
	    target.faked = true
	    return ret + 1
	 else return ret end
      end
      for i,v in ipairs(target.commands) do
	 print(v)
	 if(targets_built) then
	    local pstr = ("(%i/%i, %i%%)\r"):format(targets_built+1, targets_total,
						    math.floor(targets_built*100/targets_total+0.5))
	    erase_length = #pstr
	    io.write(pstr)
	    io.flush()
	 end
	 local success,err = os.execute(v)
	 if not success then
	    print("ERROR: Command ended with error "..err)
	    return false
	 end
      end
      target.built = true
      if(targets_built) then
	 targets_built = targets_built + 1
      end
      return true
   end
end

success,helper = pcall(require, "subcritical_helper")
if(not success or not helper) then
   if(not targets["subcritical_helper"]) then
      print("Error: subcritical_helper not found, and not equipped to build it!")
      print("Build and install SubCritical proper before building any other packages.")
      os.exit(1)
   end
   print("*** subcritical_helper not found -- unconditionally trying to build it.")
   if(not build("subcritical_helper")) then
      print("ERROR: Failed to build subcritical_helper")
      os.exit(1)
   end
   success,helper = pcall(require, "subcritical_helper")
   if(not success or not helper) then
      print("Couldn't load subcritical_helper even after trying to build it.")
      if(helper) then print(helper)
      else print("It loaded, but failed to return itself. (?!?!?!)") end
      os.exit(1)
   end
   print("*** Okay, we have subcritical_helper now. Build as normal.")
end

local function eval_target(name,target)
   if(target.eval) then return assert(target.mtime) end
   if(not target.deps) then
      target.built = true
      target.eval = true
      return target.mtime
   elseif (target.mtime) then
      local mtime = target.mtime
      target.built = true
      for i,dep in ipairs(target.deps) do
	 local nu_mtime = eval_target(dep, assert(real_targets[dep]))
	 if(nu_mtime > mtime) then
	    mtime = nu_mtime
	    target.built = false
	 end
      end
      target.eval = true
      target.mtime = mtime
      return mtime
   else
      target.built = false
      local mtime = 0
      for i,dep in ipairs(target.deps) do
	 local nu_mtime = eval_target(dep, assert(real_targets[dep] or real_targets[virtual_targets[dep]]))
	 if(nu_mtime > mtime) then
	    mtime = nu_mtime
	 end
      end
      target.mtime = mtime
      target.eval = true
      return mtime
   end
end
for name,target in pairs(real_targets) do
   local t = helper.stat(name)
   if(t) then target.mtime = assert(t.mtime)
   elseif (not target.deps) then
      print("File "..name.." with no dependencies not found!")
      os.exit(1)
   end
end
for name,target in pairs(real_targets) do
   eval_target(name,target)
end

if(os.getenv("SCBUILD_DEBUG")) then
   for k,target in pairs(real_targets) do
      if(not target.built) then print("OUTDATED: "..k) end
   end
end

local targets_to_build = {}
if(not arg or #arg == 0) then
   targets_to_build[1] = "all"
else
   for i,v in ipairs(arg) do
      if(v == "clean" and i ~= 1) then
	 print("ERROR: If clean is given, it must be the only target or the first target.")
	 os.exit(1)
      end
   end
   for i,v in ipairs(arg) do
      if(v ~= "clean") then
	 if(not fake_targets[v] and not virtual_targets[v] and not real_targets[v]) then
	    print("ERROR: "..v..": unknown target")
	    os.exit(1)
	 end
	 targets_to_build[#targets_to_build+1] = v
      end
   end
end

targets_total = 0
for i,target in ipairs(targets_to_build) do
   targets_total = targets_total + build(target, true)
end
targets_built = 0

for i,target in ipairs(targets_to_build) do
   if(not build(target)) then
      print("ERROR: Target "..target.." failed!")
      os.exit(1)
   end
end

if(erase_length) then
   io.write((" "):rep(erase_length).."\r")
   io.flush()
end
