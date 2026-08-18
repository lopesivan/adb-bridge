-- 03-exemplo.lua
local adb = require("adb-bridge")
local out = adb.shell("ls /storage/6166-6564/book")
print(out)
