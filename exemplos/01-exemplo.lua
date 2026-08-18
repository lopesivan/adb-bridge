-- a.lua
local adb = require("adb-bridge")
local out = adb.shell("ls /sdcard")
print(out)
