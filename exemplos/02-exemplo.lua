-- 02-exemplo.lua
local adb = require("adb-bridge")
local out = adb.shell("ls /storage/6166-6564")
print(out)
