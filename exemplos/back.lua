-- 03-exemplo.lua
local adb = require("adb-bridge")
local out = adb.shell("input keyevent KEYCODE_BACK")
print(out)
