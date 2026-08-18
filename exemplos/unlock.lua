-- 03-exemplo.lua
local adb = require("adb-bridge")
local out = adb.shell("input tap 360 750")
local out = adb.shell("input tap 360 750")
local out = adb.shell("input tap 180 900")
local out = adb.shell("input tap 180 900")
local out = adb.shell("input keyevent KEYCODE_ENTER")
print(out)
