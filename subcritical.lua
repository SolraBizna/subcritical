-- What a piece of junk!

--[[
  This source file is part of the SubCritical kernel.
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

collectgarbage("stop")

-- ensure that a compatible interpreter is running
do
   local major, minor = string.match(_VERSION or "", "([0-9]+)%.([0-9]+)")
   major = tonumber(major)
   minor = tonumber(minor)
   if not major or not minor then
      io.stderr:write("WARNING:\nCan't identify the running Lua version. SubCritical requires Lua 5.2.\n")
   elseif major < 5 or major == 5 and minor < 2 then
      io.stderr:write("Your Lua interpreter is too old. Lua 5.2 is required, but you have ".._VERSION..".\nPlease install Lua 5.2 before attempting to build SubCritical.\n")
      os.exit(1)
   elseif major > 5 or minor > 3 then
      io.stderr:write("WARNING:\nYour Lua interpreter may be too new. SubCritical is designed for Lua 5.2,\nbut you have ".._VERSION..". There may be incompatibilities.\n")
   end
end

-- deprecated function compatibility-with-warnings
function loadstring(...)
   io.stderr:write("WARNING:\nThis game calls loadstring. Any such calls should be replaced with a call to\nload.\n")
   loadstring = load -- only warn once
   return load(...)
end
function unpack(...)
   io.stderr:write("WARNING:\nThis game calls unpack. Any such calls should be replaced with a call to\ntable.unpack.\n")
   unpack = table.unpack -- only warn once
   return table.unpack(...)
end

local debugging = not not os.getenv("SUBCRITICAL_DEBUG")
local hardcheck = not not os.getenv("SUBCRITICAL_HARD_CHECK")
local impcheck = not not os.getenv("SUBCRITICAL_IMP_CHECK")
local pathcheck = not not os.getenv("SUBCRITICAL_PATH_CHECK")

local dprintf,printf
if(debugging) then
   function dprintf(format, ...)
      print("SCDBG:"..format:format(...))
   end
else
   function dprintf() end
end
function printf(format, ...)
   print(format:format(...))
end

function assertf(what, err, ...)
   if(not what) then error(err:format(...), 2)
   else return what end
end

function errorf(err, ...)
   error(err:format(...), 2)
end

subcritical = {}
subcritical.version = 0x0c2
subcritical.copyright = ("SubCritical %03x \194\169 2008-2014 Solra Bizna."):format(subcritical.version)
-- aliases
SubCritical,SC,sc = subcritical,subcritical,subcritical

dprintf("SubCritical %03x is being loaded.", subcritical.version)

if arg and not gamelicense then
   local n = 0
   while arg[n] do
      if arg[n] == "-lsubcritical" then
         printf("REPL mode activated, defaulting to Compatible license")
         gamelicense = "Compatible"
         break
      end
      n = n - 1
   end
end

if(not gamelicense) then
   printf("WARNING: Game license not specified!\nAdd gamelicense = \"Compatible\", gamelicense = \"GPL\", or gamelicense = \"Incompatible\" to the top of your SCG file.")
end
local gamelicense = gamelicense or "Compatible"
local licenses = {"Incompatible", "Compatible", "GPL"}
for i,v in ipairs(licenses) do
   licenses[v] = true
end
if(type(gamelicense) ~= "string" or not licenses[gamelicense]) then
   errorf("Invalid game license specified (must be %s)", table.concat(licenses, " or "))
end

local function incompatible_licenses(license1, license2)
   return ((license1 == "Incompatible" and license2 == "GPL") or
	   (license2 == "Incompatible" and license1 == "GPL"))
end

function SC.GameIsCompatible(license)
   return not incompatible_licenses(gamelicense, license)
end

dprintf("Load subcritical_helper...")
local helper = require "subcritical_helper"

if(not arg or not arg[1]) then
   arg = arg or {}
   if(os.getenv("SUBCRITICAL_COMMAND_LINE")) then
      dprintf("Parse SUBCRITICAL_COMMAND_LINE...")
      helper.parse_command_line(arg, os.getenv("SUBCRITICAL_COMMAND_LINE"))
   end
end
if(not arg[0]) then
   local level = 2
   local t
   repeat
      t = debug.getinfo(level, "S")
      if(not t) then break
      elseif (t.what ~= "tail" and t.what ~= "C" and t.source:sub(1,1) == "@") then break
      end
      level = level + 1
   until false
   if(t) then
      arg[0] = t.source:sub(2,-1)
      dprintf("Fake arg[0]: %s", arg[0])
   else
      dprintf("Couldn't make a fake arg[0]. Let's hope nobody notices.")
   end
end

local dirfrob
local dirsep,baseexp,dirfrob
if(helper.os == "windows") then
   dirsep,baseexp = "\\","\\[^\\]+$"
   local frob_t = {["/"]="\\",["\\"]="/"}
   function dirfrob(path) return path:gsub("[\\/]",frob_t) end
else dirsep,baseexp = "/","/[^/]+$" end

local function possible_path(env, default)
   if(not env) then return default
   else return env:gsub(";;+",";"..default..";"):gsub("^;",""):gsub(";$","")
   end
end

dprintf("Groping environment...")
local default_exec_path
if(helper.os == "windows") then default_exec_path = (os.getenv("USERPROFILE") or "C:\\Documents and Settings\\User").."\\SubCritical\\lib\\;C:\\SubCritical\\lib\\"
else default_exec_path = (os.getenv("HOME") or "/home").."/.subcritical/lib/;/usr/local/subcritical/lib/;/usr/subcritical/lib/;/opt/subcritical/lib/" end
local exec_path = possible_path(os.getenv("SUBCRITICAL_EXEC_PATH"), default_exec_path)
if(pathcheck) then dprintf("exec_path=%s", exec_path) end
-- SUBCRITICAL_DATA_PATH? Some day, maybe? (hence the code reuse above)

local default_config_dir
if(helper.os == "windows") then default_config_dir = (os.getenv("USERPROFILE") or "C:\\Documents and Settings\\User").."\\SubCritical\\Configuration\\"
--elseif (helper.os == "macosx") then default_config_dir = (os.getenv("HOME") or "/home").."/Library/Preferences/SubCritical/"
else default_config_dir = (os.getenv("HOME") or "/home").."/.subcritical/config/" end
local config_dir = os.getenv("SUBCRITICAL_CONFIG_DIR") or default_config_dir
if(config_dir:sub(-1,-1) ~= dirsep) then config_dir = config_dir .. dirsep end
if(pathcheck) then dprintf("config_dir=%s", config_dir) end

local default_plugin_dir
if(helper.os == "windows") then default_plugin_dir = (os.getenv("USERPROFILE") or "C:\\Documents and Settings\\User").."\\SubCritical\\Plugins\\"
else default_plugin_dir = (os.getenv("HOME") or "/home").."/.subcritical/plugins/" end
local plugin_dir = os.getenv("SUBCRITICAL_PLUGIN_DIR") or default_plugin_dir
if(plugin_dir:sub(-1,-1) ~= dirsep) then plugin_dir = plugin_dir .. dirsep end
if(pathcheck) then dprintf("plugin_dir=%s", plugin_dir) end

local so_extension = helper.so_extension

local function listallfiles(path, ext)
   local ret
   for dir in path:gmatch("[^;]+") do
      if(dir:sub(-1,-1) ~= dirsep) then
	 ret = helper.listfiles(dir..dirsep, ext, false, ret)
      else
	 ret = helper.listfiles(dir, ext, false, ret)
      end
   end
   return ret or {}
end

local classes,utilities = {},{}

dprintf("Parsing packages...")
do
   local curlicense
   local versionset
   local function parsepkgline(t, file, lineno)
      if(t[1] == "class") then
	 if(incompatible_licenses(curlicense, gamelicense)) then return end
	 assertf(t[2], "%s:%i: no class name", file, lineno)
	 local class = {}
	 class.name = t[2]
	 class.file = file:sub(1,-5)..so_extension
	 class.declscp = file
	 class.declno = lineno
	 if(classes[t[2]]) then
	    dprintf("%s:%i: Duplicate class \"%s\"", file, lineno, class.name)
	    dprintf("%s:%i: previous declaration was here", classes[t[2]].declscp, classes[t[2]].declno)
	    dprintf("%s:%i: Ignoring the new one", file, lineno)
	    return
	 end
	 local pos = 3
	 if(t[2] == "Object") then
	    -- Do nothing.
	 elseif (t[pos] == ":") then
	    pos = pos + 1
	    assertf(t[pos], "%s:%i: no superclass name for \"%s\"", file, lineno, t[2])
	    class.superclass = t[pos]
	    pos = pos + 1
	 else
	    class.superclass = "Object"
	 end
	 if(t[pos] == "concrete") then
	    pos = pos + 1
	    class.tangible = true
	    if(t[pos] == "@" or t[pos] == "priority") then
	       pos = pos + 1
	       assertf(t[pos] and tonumber(t[pos]), "%s:%i: invalid priority for \"%s\"", file, lineno, t[2])
	       class.priority = tonumber(t[pos])
	       pos = pos + 1
	    else
	       class.priority = 0
	    end
	 elseif (t[pos] == "tangible") then
	    pos = pos + 1
	    class.tangible = true
	 end
	 assertf(not t[pos], "%s:%i: Garbage tokens at end of class declaration (did you get the order wrong?)", file, lineno)
	 classes[t[2]] = class
      elseif (t[1] == "depend") then
	 if(incompatible_licenses(curlicense, gamelicense)) then return end
	 assertf(t[2], "%s:%i: no class name", file, lineno)
	 local class = {}
	 class.name = false
	 class.file = file:sub(1,-5)..so_extension
	 class.declscp = file
	 class.declno = lineno
	 class.superclass = t[2]
	 assertf(not t[3], "%s:%i: Garbage tokens at end of depend declaration", file, lineno)
	 classes[#classes+1] = class
      elseif (t[1] == "version") then
	 assertf(t[2], "%s:%i: Dangling utility token", file, lineno)
	 assertf(t[2]:match("^[0-9a-fA-F]+$"), "%s:%i: Invalid version given", file, lineno)
	 local vnum = tonumber("0x"..t[2])
	 local core
	 local pos = 3
	 if(t[pos] == "core") then
	    pos = pos + 1
	    core = true
	 end
	 if(vnum < subcritical.version and core) then
	    errorf("%s:%i: Stale core package, reinstall from latest source!", file, lineno)
--[=[
	 elseif (vnum < --[[min_version]]) then
	    errorf("%s:%i: Package too old!")
      ]=]
	 elseif (vnum > subcritical.version) then
	    errorf("%s:%i: Package designed for a later version of SubCritical (%03x)!", file, lineno, vnum)
	 end
	 assertf(not t[pos], "%s:%i: Garbage tokens at end of version declaration", file, lineno)
      elseif (t[1] == "utility") then
	 if(incompatible_licenses(curlicense, gamelicense)) then return end
	 assertf(t[2], "%s:%i: Dangling utility token", file, lineno)
	 assertf(not t[3], "%s:%i: Garbage tokens at end of utility declaration", file, lineno)
	 utilities[t[2]] = file:sub(1,-5)..so_extension
      elseif (t[1] == "license") then
	 assertf(not t[pos], "%s:%i: Garbage tokens at end of license declaration", file, lineno)
	 assertf(licenses[t[2]], "%s:%i: Invalid license specified (must be %s)", file, lineno, table.concat(licenses, " or "))
	 curlicense = t[2]
      else
	 errorf("%s:%i: Unknown SCP token: %s", file, lineno, t[1])
      end
   end
   local files = listallfiles(exec_path, ".scp")
   for i,v in ipairs(files) do
      local f = assert(io.open(v, "rb"))
      local no = 1
      -- hack to make all packages depend on core
      local class = {}
      class.name = false
      class.file = v:sub(1,-5)..so_extension
      class.declscp = file
      class.declno = lineno
      class.superclass = "Object"
      classes[#classes+1] = class
      -- end hack
      for l in f:lines() do
	 local t = {}
	 for tok in l:gsub("//.*$", ""):gmatch("[^ \t\r\n]+") do
	    t[#t+1] = tok
	 end
	 if(t[1] ~= "license" and no == 1) then
	    curlicense = "Compatible"
	    dprintf("WARNING: %s does not explicitly specify license in the first line, defaulting to GPL compatible", v)
	 end
	 if(t[1]) then parsepkgline(t, v, no) end
	 no = no + 1
      end
      f:close()
   end
end

dprintf("Checking package tree consistency...")
assert(classes.Object, "No Object class was defined! Please reinstall SubCritical or check your SUBCRITICAL_EXEC_PATH variable.")
do
   local err = 0
   repeat
      local removed
      for name,v in pairs(classes) do
	 if(v.superclass and not classes[v.superclass]) then
	    classes[name] = nil
	    removed = true
	    if(v.name) then
	       printf("Class \"%s\" (%s:%i) inherits from unknown superclass \"%s\"", v.name, v.declscp, v.declno, v.superclass)
	    else
	       printf("Dependency \"%s\" (%s:%i) could not be resolved", v.superclass, v.declscp, v.declno)
	    end
	    err = err + 1
	 end
      end
   until not removed
   if(err > 0) then
      print("Class tree incomplete ("..err.." missing)")
      os.exit(1)
   end
end
for name,class in pairs(classes) do
   local seen = {[name]=true}
   local us = classes[class.superclass]
   while(us) do
      assertf(not seen[us.name], "Class \"%s\" inherits from itself!", us.name)
      seen[us.name] = true
      us = classes[us.superclass]
   end
end
for name,v in pairs(classes) do
   local us = v
   assert(us)
   while(us and us.superclass and us.file == v.file) do
      us = assert(classes[us.superclass])
   end
   assert(us)
   if(us.superclass) then
      while(us and us.superclass and us.file ~= v.file) do
	 us = assert(classes[us.superclass])
      end
      assertf(us.file ~= v.file, "%s:%i: Obvious circular dependency starting at class %s", v.declscp, v.declno, v.name)
   end
end

dprintf("Calculating package file dependencies...")
local filedeps = {}
-- this code is confusing, I'm sorry
for name,class in pairs(classes) do
   local v = class
   while(true) do
      local deps
      if(not filedeps[v.file]) then deps = {} filedeps[v.file] = deps
      else deps = filedeps[v.file] end
      local us = v
      assert(us)
      while(us and us.superclass and us.file == v.file) do
	 us = assert(classes[us.superclass])
      end
      assert(us)
      if(us.file ~= v.file) then
	 if(not deps[us.file]) then
	    deps[us.file] = true
	    deps[#deps+1] = us.file
	 end
	 v = us
      else
	 break
      end
   end
end
for file,deps in pairs(filedeps) do
   local checked = {}
   local function checkdeps(deps)
      for i,v in ipairs(deps) do
	 if(not checked[v]) then
	    assertf(v ~= file, "Less obvious circular dependency starting at file %s", file)
	    checked[v] = true
	    if(filedeps[v]) then
	       checkdeps(filedeps[v])
	    end
	 end
      end
   end
   for i,v in ipairs(deps) do
      if(filedeps[v]) then
	 checkdeps(filedeps[v])
      end
   end
end

dprintf("Removing fake (depend) classes...")
for n=#classes,1,-1 do
   classes[n] = nil
end

dprintf("Building implementor list...")
do
   for name,class in pairs(classes) do
      if(class.priority) then
	 local up = class
	 while(up and up.name ~= "Object") do
	    local imps
	    if(up.imps) then imps = up.imps
	    else imps = {} up.imps = imps end
	    imps = imps or {}
	    imps[#imps+1] = class
	    up = classes[up.superclass]
	 end
      end
   end
   for name,class in pairs(classes) do
      if(class.imps) then
	 table.sort(class.imps, function(a,b)
				   return a.priority < b.priority
				end)
	 if(impcheck) then
	    local t = {}
	    for i,v in ipairs(class.imps) do
	       t[i] = v.name
	    end
	    dprintf("  %s -> %s", name, table.concat(t, ", "))
	 end
      end
   end
end

if(hardcheck) then
   dprintf("Package list:")
   for name,class in pairs(classes) do
      dprintf("  %s (%s:%i <- %s)", name, class.declscp, class.declno, class.file)
      if(class.superclass) then dprintf("    is a "..class.superclass) end
      if(not class.tangible) then dprintf("    is abstract")
      elseif (not class.priority) then dprintf("    is tangible")
      else dprintf("    is concrete; priority %i", class.priority) end
   end
end

dprintf("Fudging the path...")
do
   local nupath = package.path
   --Leaving this out is harmless, leaving it in is hacky.
   --nupath = nupath:gsub("%.[/\\]%?%.lua","")
   local t = {}
   if(arg and arg[0] and arg[0]:match(dirsep)) then
      local target = arg[0]:gsub(baseexp,dirsep)
      t[#t+1] = target.."?.lua"
      t[#t+1] = target.."?.scu"
      t[#t+1] = target.."?"..dirsep.."package.scu"
      t[#t+1] = target.."lib"..dirsep.."?.lua"
      t[#t+1] = target.."lib"..dirsep.."?.scu"
      t[#t+1] = target.."lib"..dirsep.."?"..dirsep.."package.scu"
   else
      t[#t+1] = "?.lua"
      t[#t+1] = "?.scu"
      t[#t+1] = "?"..dirsep.."package.scu"
      t[#t+1] = "lib"..dirsep.."?.lua"
      t[#t+1] = "lib"..dirsep.."?.scu"
      t[#t+1] = "lib"..dirsep.."?"..dirsep.."package.scu"
   end
   for dir in exec_path:gmatch("[^;]+") do
      if(dir:sub(-1,-1) ~= dirsep) then
	 t[#t+1] = dir .. dirsep .. "?.scu"
	 t[#t+1] = dir .. dirsep .. "?"..dirsep.."package.scu"
      else
	 t[#t+1] = dir .. "?.scu"
	 t[#t+1] = dir .. "?"..dirsep.."package.scu"
      end
   end
   t[#t+1] = nupath
   nupath = table.concat(t, ";")
   nupath = nupath:gsub(";;+",";"):gsub("^;",""):gsub(";$","")
   if(pathcheck) then
      dprintf("  OLD: %s", package.path)
      dprintf("  NEW: %s", nupath)
   end
   package.path = nupath
end

local loadedfiles = {}
local function readso(file, symbol)
   return helper.dlsym(assert(loadedfiles[file]), symbol)
end

local function loadso(file)
   if(loadedfiles[file]) then return loadedfiles[file] end
   if(filedeps[file]) then
      for i,v in ipairs(filedeps[file]) do
	 loadso(v)
      end
   end
   dprintf("Opening %s", file)
   loadedfiles[file] = assert(helper.dlopen(file))
   local i = readso(file, "Init_"..assert(file:match("([^%./\\]+)%.[^%./]+$")))
   if(i) then dprintf("  Calling initializer") i() end
end

local function ensure_loaded(class)
   assert(class)
   if(class.loaded) then return true end
   if(class.superclass) then ensure_loaded(classes[class.superclass]) end
   loadso(assert(class.file))
   class.loaded = true
end

local function construct(class, ...)
   if(class.constructor) then return class.constructor(...) end
   ensure_loaded(class)
   class.constructor = assert(readso(class.file, "Construct_"..class.name))
   return class.constructor(...)
end

local function ck_construct(class)
   if(class.constructor) then return true end
   ensure_loaded(class)
   return readso(class.file, "Construct_"..class.name)
end

scutil = {}
SCUtil,subcritical.Util,subcritical.util = scutil,scutil,scutil
local scutil = scutil
local scutil_mt = {
   __index=function(t,k)
	      if(rawget(t,k)) then return rawget(t,k) end
	      assertf(utilities[k], "Attempt to access unknown utility '%s'", k)
	      loadso(utilities[k])
	      local func = readso(utilities[k], "Utility_"..k)
	      rawset(t,k,func)
	      return func
	   end,
}
setmetatable(scutil, scutil_mt)

function subcritical.Construct(class, ...)
   local override = os.getenv("SC_Override_"..class)
   if override and #override > 0 then
      if classes[override] then
         return construct(classes[override], ...)
      else
         error("Environment-specified override for "..class.." references a class ("..override..") that doesn't exist!")
      end
   end
   if(not classes[class]) then error("Unknown class: "..class, 2) end
   if(not classes[class].tangible) then error("Class "..class.." is not tangible") end
   if(not classes[class].imps) then error("Class "..class.." has no available implementations.") end
   return construct(classes[class].imps[1], ...)
end
function subcritical.ConstructSpecific(class, ...)
   if(not classes[class]) then error("Unknown class: "..class, 2) end
   if(not classes[class].priority) then error("Class "..class.." is not concrete", 2) end
   return construct(classes[class], ...)
end
local construct_specific = subcritical.ConstructSpecific
function subcritical.ConstructPath(path, level)
   level = level or 2
   if(dirfrob) then path = dirfrob(path) end
   local t
   repeat
      t = debug.getinfo(level, "S")
      if(not t) then return construct_specific("Path", path)
      elseif (t.what ~= "tail" and t.what ~= "C" and t.source:sub(1,1) == "@") then break
      end
      level = level + 1
   until false
   local source = t.source:sub(2,-1)
   if(source:match(dirsep)) then
      return construct_specific("Path", assert(source:gsub(baseexp,dirsep))..path)
   else
      return construct_specific("Path", path)
   end
end
function SubCritical.ConstructRelativePath(sourcepath, path)
   assert(sourcepath, "sourcepath not given")
   local curpath = sourcepath:GetPath()
   for component in path:gmatch("[^/]+") do
      if component == "." then
      elseif component == ".." then
         curpath = curpath .. dirsep .. ".."
      elseif dirfrob then
         curpath = curpath .. dirsep .. dirfrob(component)
      else
         curpath = curpath .. dirsep .. component
      end
   end
   return construct_specific("Path",curpath)
end
SubCritical.RelPath = SubCritical.ConstructRelativePath
function SubCritical.ListFiles(extension, dirpath, dirs)
   dirpath = dirpath or SubCritical.Path(".", 3)
   local rawpath = dirpath:GetPath() .. dirsep
   local ret = {}
   local files = helper.listfiles(rawpath, extension or "", dirs)
   table.sort(files)
   for n=1,#files do
      ret[n] = SubCritical.Construct("Path", files[n])
   end
   return ret
end
function SubCritical.ListFilesRecursive(extension, dirpath, dirs)
   dirpath = dirpath or SubCritical.Path(".", 3)
   local rawpath = dirpath:GetPath() .. dirsep
   local ret = {}
   local function read_dir(path)
      local files,dirs = helper.listfiles_plusdirs(path, extension or "", dirs)
      if not files then return end
      table.sort(files)
      table.sort(dirs)
      for n=1,#dirs do
	 read_dir(path..dirs[n]..dirsep)
      end
      for n=1,#files do
	 ret[#ret+1] = SubCritical.Construct("Path", path..files[n])
      end
   end
   read_dir(rawpath)
   return ret
end
function SubCritical.ListFolders(extension, dirpath)
   return SC.ListFiles(extension, dirpath, true)
end
function SubCritical.ListFoldersRecursive(extension, dirpath)
   return SC.ListFilesRecursive(extension, dirpath, true)
end
-- more aliases
subcritical.Path = subcritical.ConstructPath
scpath,SCPath,P = subcritical.Path,subcritical.Path,subcritical.Path
-- while we're at it, make io.open obey our new regime
local old_open = io.open
function io.open(path, mode, ...)
   if(type(path) ~= "userdata" or path:Identity() ~= "Path") then
      if sc.no_strict_path then
         return old_open(path, mode, ...)
      else
         error("Now that SubCritical has control, io.open takes a Path parameter instead of a string.\nWhere before you would say: io.open(path, \"r\")\nTry: io.open(SCPath(path), \"r\")\nIf fixing your code is impossible, set SubCritical.no_strict_path to true.", 1)
      end
   end
   if(not mode) then mode = "rb"
   elseif (not mode:match("b")) then mode = mode .. "b" end
   return old_open(path:GetPath(), mode, ...)
end
-- and the old dofile and loadfile
local old_dofile = dofile
function dofile(path, ...)
   if(type(path) ~= "userdata" or path:Identity() ~= "Path") then
      if sc.no_strict_path then
         return old_dofile(path, ...)
      else
         error("Now that SubCritical has control, dofile takes a Path parameter instead of a string.\nWhere before you would say: dofile(path)\nTry: dofile(SCPath(path))\nIf fixing your code is impossible, set SubCritical.no_strict_path to true.", 1)
      end
   end
   return old_dofile(path:GetPath(), ...)
end
local old_loadfile = loadfile
function loadfile(path, ...)
   if(type(path) ~= "userdata" or path:Identity() ~= "Path") then
      if sc.no_string_path then
         return old_loadfile(path, ...)
      else
         error("Now that SubCritical has control, loadfile takes a Path parameter instead of a string.\nWhere before you would say: loadfile(path)\nTry: loadfile(SCPath(path))\nIf fixing your code is impossible, set SubCritical.no_strict_path to true.", 1)
      end
   end
   return old_loadfile(path:GetPath(), ...)
end
-- and let's give math.random some guaranteed properties, if we can
if(classes.RNG) then
   local global_rng = SC.Construct("RNG", 1)
   function math.randomseed(...)
      global_rng = SC.Construct("RNG", ...)
   end
   function math.random(...)
      return global_rng:Random(...)
   end
end

function subcritical.Choices(class, ...)
   if(not classes[class]) then error("Unknown class: "..class, 2) end
   if(classes[class].friendly_imps) then return classes[class].friendly_imps end
   local t = {}
   classes[class].friendly_imps = t
   for i,v in ipairs(classes[class].imps) do
      t[i] = v.name
   end
   return t
end

function subcritical.ParseCommandLine(cmdline)
   local ret = {}
   helper.parse_command_line(ret, cmdline)
   return ret
end

local function fixvarname(name)
   assert(name:sub(1,1) ~= ".", "Var file names must not begin with '.'")
   assert(name:sub(-1,-1) ~= "~", "Var file names must not end with '~'")
   name = name:gsub("/", "_")
   if(dirsep ~= "/") then name = name:gsub(dirsep, "/") end
   if(dirfrob) then name = dirfrob(name) end
   return name
end

local fpmt = {
   __index={}, __newindex=function(t,k,v) error("don't be silly") end,
}
function fpmt.__index:read(...) return rawget(self,"file"):read(...) end
function fpmt.__index:write(...) return rawget(self,"file"):write(...) end
function fpmt.__index:lines(...) return rawget(self,"file"):lines(...) end
function fpmt.__index:seek(...) return rawget(self,"file"):seek(...) end
function fpmt.__index:setvbuf(...) return rawget(self,"file"):setvbuf(...) end
function fpmt.__index:flush(...) return rawget(self,"file"):flush(...) end
function fpmt.__index:close(...)
   local ret = {rawget(self,"file"):close(...)}
   helper.replace(rawget(self,"dname"),rawget(self,"sname"))
   return table.unpack(ret)
end

local function cproxy(t)
   setmetatable(t, fpmt)
   return t
end

function sc.GetPluginPath(name)
   local path = plugin_dir..fixvarname(name)..dirsep
   helper.ckdir(path)
   return SC.Construct("Path", path)
end

sc.var = {}
SCVar,scvar,sc.Var = sc.var,sc.var,sc.var
local function confdir(config_dir,...)
   assert(..., "no name provided")
   local buildup = {config_dir,...}
   local name = table.remove(buildup,#buildup)
   if #buildup >= 2 then
      for n=2,#buildup do
         buildup[n] = fixvarname(buildup[n])
      end
      return table.concat(buildup,dirsep)..dirsep,name
   else
      return config_dir,name
   end
end
function sc.var.Read(...)
   local config_dir,name = confdir(config_dir,...)
   name = config_dir..fixvarname(name)
   return (old_open(name,"rb"))
end
function sc.var.Write(...)
   local config_dir,name = confdir(config_dir,...)
   helper.ckdir(config_dir)
   name = config_dir..fixvarname(name)
   return cproxy({file=old_open(name.."~","w+b"), sname=name.."~", dname=name})
end
function sc.var.Update(...)
   local config_dir,name = confdir(config_dir,...)
   helper.ckdir(config_dir)
   name = config_dir..fixvarname(name)
   helper.copy(name.."~", name) -- save memory by not doing this in Lua
   return cproxy({file=old_open(name.."~","a+b"), sname=name.."~", dname=name})
end
function sc.var.Path(...)
   local config_dir,name = confdir(config_dir,...)
   if name then
      name = config_dir..fixvarname(name)
   else
      name = config_dir
   end
   return SC.Construct("Path",name)
end
function sc.var.Remove(...)
   local config_dir,name = confdir(config_dir,...)
   name = config_dir..fixvarname(name)
   os.remove(name)
   os.remove(name.."~")
end

if(hardcheck) then
   local failed = 0
   dprintf("Hard check requested.")
   dprintf("  Trying to get constructors for all concrete classes.")
   for name,class in pairs(classes) do
      if(class.priority) then
	 local s,e = ck_construct(class)
	 if(not s) then
	    dprintf("    %s", e)
	    failed = failed + 1
	 else
	    dprintf("    %s: OK", name)
	 end
      end
   end
   dprintf("  Trying to access all utilities.")
   for name in pairs(utilities) do
      local s,e = pcall(function() local n = assert(scutil[name], "not found") end)
      if(s) then
	 dprintf("    %s: OK", name)
      else
	 dprintf("    %s: FAIL (%s)", name, e)
	 failed = failed + 1
      end
   end
   if(failed > 0) then
      if(not debugging) then
	 errorf("Error count of %i while doing package hard check -- set SUBCRITICAL_DEBUG=1 for more info.", failed)
      else
	 dprintf("Error count: %i", failed)
	 dprintf("Hard check failed! Check your symbols!")
	 os.exit(1)
      end
   end
   dprintf("Hard check passed.")
end

do
   local before,after
   before = collectgarbage("count")
   collectgarbage("collect")
   after = collectgarbage("count")
   dprintf("Collected %.1fKiB of garbage.", before - after)
   collectgarbage("restart")
end

dprintf("Passing control to game.")
