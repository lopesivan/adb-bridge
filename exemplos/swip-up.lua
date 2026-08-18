-- 03-exemplo.lua
local adb = require("adb-bridge")
local out = adb.shell("input swipe 540 1800 540 400 300")
print(out)
