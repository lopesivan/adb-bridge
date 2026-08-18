-- lua/adb-bridge/init.lua
local M = {}
local ffi = require("ffi")

ffi.cdef([[
    char *adb_devices(void);
    char *adb_devices_l(void);
    char *adb_version(void);

    char *adb_shell(const char *serial, const char *cmd);
    char *adb_get_state(const char *serial);
    int adb_root(const char *serial, char **out_message);
    int adb_unroot(const char *serial, char **out_message);
    int adb_remount(const char *serial, char **out_message);
    int adb_reboot(const char *serial, const char *mode, char **out_message);
    int adb_tcpip(const char *serial, int port, char **out_message);
    int adb_forward(const char *serial, const char *local_spec,
                    const char *remote_spec, char **out_message);

    int adb_push(const char *serial, const char *local_path,
                 const char *remote_path, char **out_message);
    int adb_pull(const char *serial, const char *remote_path,
                 const char *local_path, char **out_message);

    void adb_free(char *ptr);
]])

local lib
local config = {
  lib_path = nil,
  serial = nil,
}

local function find_lib_path()
  local src = debug.getinfo(1, "S").source:sub(2)
  local plugin_root = src:match("(.*/)lua/adb%-bridge/init%.lua$")
  return plugin_root and (plugin_root .. "adb_bridge.so") or nil
end

local function c_serial()
  return config.serial
end

local function ensure_lib()
  if lib then
    return true
  end
  vim.notify("[adb-bridge] setup() não foi chamado", vim.log.levels.ERROR)
  return false
end

local function take_and_free(ptr)
  if ptr == nil then
    return nil
  end
  local s = ffi.string(ptr)
  lib.adb_free(ptr)
  return s
end

local function run_bool_cmd(title, fn)
  if not ensure_lib() then
    return false
  end
  local msg_ptr = ffi.new("char*[1]")
  local ok = fn(msg_ptr) == 1
  local msg = take_and_free(msg_ptr[0]) or ""
  M.show_output(title .. (ok and " (ok)" or " (falhou)"), msg)
  return ok, msg
end

function M.setup(opts)
  config = vim.tbl_deep_extend("force", config, opts or {})

  local lib_path = config.lib_path or find_lib_path()
  if not lib_path then
    vim.notify("[adb-bridge] não foi possível localizar adb_bridge.so", vim.log.levels.ERROR)
    return
  end

  local ok, result = pcall(ffi.load, lib_path)
  if not ok then
    vim.notify("[adb-bridge] falha ao carregar " .. lib_path .. ": " .. tostring(result),
      vim.log.levels.ERROR)
    return
  end
  lib = result

  local command = vim.api.nvim_create_user_command

  command("AdbShell", function(o) M.shell(o.args) end,
    { nargs = "+", desc = "Executa adb shell via bridge nativo" })
  command("AdbDevices", function() M.devices() end, { desc = "Lista dispositivos" })
  command("AdbDevicesL", function() M.devices_l() end, { desc = "Lista dispositivos com detalhes" })
  command("AdbVersion", function() M.version() end, { desc = "Mostra versão do protocolo do adb server" })
  command("AdbState", function() M.get_state() end, { desc = "Mostra estado do device" })
  command("AdbRoot", function() M.root() end, { desc = "Reinicia adbd como root" })
  command("AdbUnroot", function() M.unroot() end, { desc = "Reinicia adbd sem root" })
  command("AdbRemount", function() M.remount() end, { desc = "Remonta partições do sistema" })
  command("AdbReboot", function(o) M.reboot(o.args) end,
    { nargs = "?", desc = "Reinicia o device [bootloader|recovery]" })
  command("AdbTcpip", function(o) M.tcpip(tonumber(o.args)) end,
    { nargs = 1, desc = "Ativa adb TCP/IP na porta indicada" })

  command("AdbForward", function(o)
    if #o.fargs ~= 2 then
      vim.notify("[adb-bridge] uso: AdbForward <local> <remote>", vim.log.levels.ERROR)
      return
    end
    M.forward(o.fargs[1], o.fargs[2])
  end, { nargs = "+", desc = "Cria port forwarding ADB" })

  command("AdbPush", function(o)
    if #o.fargs ~= 2 then
      vim.notify("[adb-bridge] uso: AdbPush <local> <remote>", vim.log.levels.ERROR)
      return
    end
    M.push(o.fargs[1], o.fargs[2])
  end, { nargs = "+", complete = "file", desc = "Envia arquivo para o device" })

  command("AdbPull", function(o)
    if #o.fargs ~= 2 then
      vim.notify("[adb-bridge] uso: AdbPull <remote> <local>", vim.log.levels.ERROR)
      return
    end
    M.pull(o.fargs[1], o.fargs[2])
  end, { nargs = "+", desc = "Copia arquivo do device" })

  command("AdbInstall", function(o) M.install(o.args) end,
    { nargs = 1, complete = "file", desc = "Instala APK usando push + pm install" })

  command("AdbInput", function(o) M.input(o.args) end, { nargs = "+", desc = "Executa input" })
  command("AdbSettings", function(o) M.settings(o.args) end, { nargs = "+", desc = "Executa settings" })
  command("AdbUiAutomator", function(o) M.uiautomator(o.args) end,
    { nargs = "+", desc = "Executa uiautomator" })
  command("AdbAm", function(o) M.am(o.args) end, { nargs = "+", desc = "Executa Activity Manager" })
  command("AdbPm", function(o) M.pm(o.args) end, { nargs = "+", desc = "Executa Package Manager" })
  command("AdbStart", function(o) M.start(o.args) end, { nargs = "+", desc = "Inicia uma Activity" })

  command("AdbTap", function(o)
    if #o.fargs ~= 2 then
      vim.notify("[adb-bridge] uso: AdbTap <x> <y>", vim.log.levels.ERROR)
      return
    end
    M.tap(o.fargs[1], o.fargs[2])
  end, { nargs = "+", desc = "Simula toque" })

  command("AdbKeyevent", function(o) M.keyevent(o.args) end, { nargs = 1, desc = "Envia keyevent" })

  command("AdbSwipe", function(o)
    if #o.fargs < 4 or #o.fargs > 5 then
      vim.notify("[adb-bridge] uso: AdbSwipe <x1> <y1> <x2> <y2> [ms]", vim.log.levels.ERROR)
      return
    end
    M.swipe(o.fargs[1], o.fargs[2], o.fargs[3], o.fargs[4], o.fargs[5])
  end, { nargs = "+", desc = "Simula gesto swipe" })

  command("AdbText", function(o) M.text(o.args) end, { nargs = "+", desc = "Digita texto" })
  command("AdbUninstall", function(o) M.uninstall(o.args) end,
    { nargs = "+", desc = "Desinstala pacote" })
end

function M.shell(cmd)
  if not ensure_lib() then return end
  local result = take_and_free(lib.adb_shell(c_serial(), cmd))
  M.show_output("adb shell " .. cmd, result)
  return result
end

function M.devices()
  if not ensure_lib() then return end
  local result = take_and_free(lib.adb_devices())
  M.show_output("adb devices", result)
  return result
end

function M.devices_l()
  if not ensure_lib() then return end
  local result = take_and_free(lib.adb_devices_l())
  M.show_output("adb devices -l", result)
  return result
end

function M.version()
  if not ensure_lib() then return end
  local result = take_and_free(lib.adb_version())
  M.show_output("adb version (server protocol)", result)
  return result
end

function M.get_state()
  if not ensure_lib() then return end
  local result = take_and_free(lib.adb_get_state(c_serial()))
  M.show_output("adb get-state", result)
  return result
end

function M.root()
  return run_bool_cmd("adb root", function(p) return lib.adb_root(c_serial(), p) end)
end

function M.unroot()
  return run_bool_cmd("adb unroot", function(p) return lib.adb_unroot(c_serial(), p) end)
end

function M.remount()
  return run_bool_cmd("adb remount", function(p) return lib.adb_remount(c_serial(), p) end)
end

function M.reboot(mode)
  mode = mode or ""
  return run_bool_cmd("adb reboot " .. mode,
    function(p) return lib.adb_reboot(c_serial(), mode, p) end)
end

function M.tcpip(port)
  if not port or port < 1 or port > 65535 then
    vim.notify("[adb-bridge] porta inválida (1..65535)", vim.log.levels.ERROR)
    return false
  end
  return run_bool_cmd("adb tcpip " .. port,
    function(p) return lib.adb_tcpip(c_serial(), port, p) end)
end

function M.forward(local_spec, remote_spec)
  return run_bool_cmd("adb forward " .. local_spec .. " " .. remote_spec,
    function(p) return lib.adb_forward(c_serial(), local_spec, remote_spec, p) end)
end

function M.push(local_path, remote_path)
  return run_bool_cmd("adb push " .. local_path .. " " .. remote_path,
    function(p) return lib.adb_push(c_serial(), local_path, remote_path, p) end)
end

function M.pull(remote_path, local_path)
  return run_bool_cmd("adb pull " .. remote_path .. " " .. local_path,
    function(p) return lib.adb_pull(c_serial(), remote_path, local_path, p) end)
end

function M.install(apk_path)
  if not apk_path or apk_path == "" then
    vim.notify("[adb-bridge] informe o caminho do APK", vim.log.levels.ERROR)
    return false
  end

  local name = vim.fs.basename(apk_path)
  local remote = "/data/local/tmp/adb-bridge-" .. name
  local ok, msg = M.push(apk_path, remote)
  if not ok then
    return false, msg
  end

  local result = M.shell("pm install -r " .. vim.fn.shellescape(remote))
  M.shell("rm -f " .. vim.fn.shellescape(remote))
  return result and result:match("Success") ~= nil, result
end

function M.input(arguments) return M.shell("input " .. arguments) end
function M.settings(arguments) return M.shell("settings " .. arguments) end
function M.uiautomator(arguments) return M.shell("uiautomator " .. arguments) end
function M.am(arguments) return M.shell("am " .. arguments) end
function M.pm(arguments) return M.shell("pm " .. arguments) end
function M.start(arguments) return M.am("start " .. arguments) end
function M.tap(x, y) return M.input(string.format("tap %s %s", x, y)) end
function M.keyevent(key) return M.input("keyevent " .. key) end

function M.swipe(x1, y1, x2, y2, duration)
  local args = string.format("swipe %s %s %s %s", x1, y1, x2, y2)
  if duration and duration ~= "" then args = args .. " " .. duration end
  return M.input(args)
end

function M.text(value) return M.input("text " .. value) end
function M.uninstall(arguments) return M.pm("uninstall " .. arguments) end

function M.show_output(title, text)
  text = text or ""
  vim.cmd("botright split")
  vim.cmd("resize 15")
  local buf = vim.api.nvim_create_buf(false, true)
  vim.api.nvim_win_set_buf(0, buf)
  pcall(vim.api.nvim_buf_set_name, buf, title)
  vim.bo[buf].buftype = "nofile"
  vim.bo[buf].bufhidden = "wipe"
  vim.bo[buf].swapfile = false

  local lines = vim.split(text, "\n", { plain = true })
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, lines)
  vim.bo[buf].modifiable = false
end

return M
