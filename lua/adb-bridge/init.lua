-- lua/adb-bridge/init.lua
-- Bridge nativo para adb server via LuaJIT FFI (implementado em C++,
-- exposto via extern "C" em src/adb_bridge_api.cpp).

local M = {}
local ffi = require("ffi")

ffi.cdef([[
    /* host: (não depende de device específico) */
    char *adb_devices(void);
    char *adb_devices_l(void);
    char *adb_version(void);

    /* transport (por device) */
    char *adb_shell(const char *serial, const char *cmd);
    char *adb_get_state(const char *serial);
    int   adb_root(const char *serial, char **out_message);
    int   adb_remount(const char *serial, char **out_message);
    int   adb_reboot(const char *serial, const char *mode, char **out_message);
    int   adb_tcpip(const char *serial, int port, char **out_message);

    /* utilitário */
    void  adb_free(char *ptr);
]])

local lib = nil
local config = {
  lib_path = nil, -- nil = busca automática ao lado deste init.lua
  serial = nil, -- nil = usa o único device conectado (host:transport-any)
}

local function find_lib_path()
  local src = debug.getinfo(1, "S").source:sub(2)
  local plugin_root = src:match("(.*/)lua/adb%-bridge/init%.lua$")
  if plugin_root then
    return plugin_root .. "adb_bridge.so"
  end
  return nil
end

-- Converte string Lua/nil em const char* seguro pra FFI (nil vira NULL).
local function c_serial()
  return config.serial
end

local function take_and_free(ptr)
  if ptr == nil then
    return nil
  end
  local s = ffi.string(ptr)
  lib.adb_free(ptr)
  return s
end

function M.setup(opts)
  config = vim.tbl_deep_extend("force", config, opts or {})

  local lib_path = config.lib_path or find_lib_path()
  if not lib_path then
    vim.notify(
      "[adb-bridge] não foi possível localizar adb_bridge.so",
      vim.log.levels.ERROR
    )
    return
  end

  local ok, result = pcall(ffi.load, lib_path)
  if not ok then
    vim.notify(
      "[adb-bridge] falha ao carregar "
        .. lib_path
        .. ": "
        .. tostring(result),
      vim.log.levels.ERROR
    )
    return
  end
  lib = result

  vim.api.nvim_create_user_command("AdbShell", function(cmdopts)
    M.shell(cmdopts.args)
  end, { nargs = "+", desc = "Executa `adb shell <cmd>` via bridge nativo" })

  vim.api.nvim_create_user_command("AdbDevices", function()
    M.devices()
  end, { desc = "Lista dispositivos (host:devices)" })

  vim.api.nvim_create_user_command("AdbDevicesL", function()
    M.devices_l()
  end, { desc = "Lista dispositivos com detalhes (host:devices-l)" })

  vim.api.nvim_create_user_command("AdbState", function()
    M.get_state()
  end, { desc = "Estado do device (host:get-state)" })

  vim.api.nvim_create_user_command("AdbRoot", function()
    M.root()
  end, { desc = "Reinicia adbd como root (adb root)" })

  vim.api.nvim_create_user_command("AdbRemount", function()
    M.remount()
  end, { desc = "Remonta /system como read-write (adb remount)" })

  vim.api.nvim_create_user_command("AdbReboot", function(cmdopts)
    M.reboot(cmdopts.args)
  end, {
    nargs = "?",
    desc = "Reinicia o device (args: vazio|bootloader|recovery)",
  })

  vim.api.nvim_create_user_command("AdbTcpip", function(cmdopts)
    M.tcpip(tonumber(cmdopts.args))
  end, {
    nargs = 1,
    desc = "Ativa adb via rede na porta dada (adb tcpip <porta>)",
  })

  -- Atalhos construídos sobre o serviço shell: já existente. Eles não
  -- exigem novas funções na biblioteca C++.
  vim.api.nvim_create_user_command("AdbInput", function(cmdopts)
    M.input(cmdopts.args)
  end, { nargs = "+", desc = "Executa input no device" })

  vim.api.nvim_create_user_command("AdbSettings", function(cmdopts)
    M.settings(cmdopts.args)
  end, { nargs = "+", desc = "Consulta ou altera settings do Android" })

  vim.api.nvim_create_user_command("AdbUiAutomator", function(cmdopts)
    M.uiautomator(cmdopts.args)
  end, { nargs = "+", desc = "Executa uiautomator no device" })

  vim.api.nvim_create_user_command("AdbAm", function(cmdopts)
    M.am(cmdopts.args)
  end, { nargs = "+", desc = "Executa o Activity Manager (am)" })

  vim.api.nvim_create_user_command("AdbPm", function(cmdopts)
    M.pm(cmdopts.args)
  end, { nargs = "+", desc = "Executa o Package Manager (pm)" })

  vim.api.nvim_create_user_command("AdbStart", function(cmdopts)
    M.start(cmdopts.args)
  end, { nargs = "+", desc = "Inicia uma Activity com am start" })

  vim.api.nvim_create_user_command("AdbTap", function(cmdopts)
    if #cmdopts.fargs ~= 2 then
      vim.notify("[adb-bridge] uso: AdbTap <x> <y>", vim.log.levels.ERROR)
      return
    end

    M.tap(cmdopts.fargs[1], cmdopts.fargs[2])
  end, {
    nargs = "+",
    desc = "Simula toque: AdbTap <x> <y>",
  })

  vim.api.nvim_create_user_command("AdbKeyevent", function(cmdopts)
    M.keyevent(cmdopts.args)
  end, { nargs = 1, desc = "Envia tecla: AdbKeyevent <código|KEYCODE_*>" })

  vim.api.nvim_create_user_command(
    "AdbSwipe",
    function(cmdopts)
      if #cmdopts.fargs < 4 or #cmdopts.fargs > 5 then
        vim.notify(
          "[adb-bridge] uso: AdbSwipe <x1> <y1> <x2> <y2> [duração_ms]",
          vim.log.levels.ERROR
        )
        return
      end

      M.swipe(
        cmdopts.fargs[1],
        cmdopts.fargs[2],
        cmdopts.fargs[3],
        cmdopts.fargs[4],
        cmdopts.fargs[5]
      )
    end,
    { nargs = "+", desc = "Simula gesto: AdbSwipe <x1> <y1> <x2> <y2> [ms]" }
  )

  vim.api.nvim_create_user_command("AdbText", function(cmdopts)
    M.text(cmdopts.args)
  end, { nargs = "+", desc = "Digita texto no device" })

  vim.api.nvim_create_user_command("AdbUninstall", function(cmdopts)
    M.uninstall(cmdopts.args)
  end, { nargs = "+", desc = "Desinstala pacote com pm uninstall" })
end

function M.shell(cmd)
  if not lib then
    vim.notify("[adb-bridge] setup() não foi chamado", vim.log.levels.ERROR)
    return
  end
  local result = take_and_free(lib.adb_shell(c_serial(), cmd))
  M.show_output("adb shell " .. cmd, result)
  return result
end

function M.devices()
  if not lib then
    return
  end
  local result = take_and_free(lib.adb_devices())
  M.show_output("adb devices", result)
  return result
end

function M.devices_l()
  if not lib then
    return
  end
  local result = take_and_free(lib.adb_devices_l())
  M.show_output("adb devices -l", result)
  return result
end

function M.get_state()
  if not lib then
    return
  end
  local result = take_and_free(lib.adb_get_state(c_serial()))
  M.show_output("adb get-state", result)
  return result
end

-- Wrappers Lua exportados. Todos reutilizam adb_shell()/shell: e, portanto,
-- não acrescentam símbolos à API C++/FFI.
function M.input(arguments)
  return M.shell("input " .. arguments)
end

function M.settings(arguments)
  return M.shell("settings " .. arguments)
end

function M.uiautomator(arguments)
  return M.shell("uiautomator " .. arguments)
end

function M.am(arguments)
  return M.shell("am " .. arguments)
end

function M.pm(arguments)
  return M.shell("pm " .. arguments)
end

function M.start(arguments)
  return M.am("start " .. arguments)
end

function M.tap(x, y)
  return M.input(string.format("tap %s %s", x, y))
end

function M.keyevent(key)
  return M.input("keyevent " .. key)
end

function M.swipe(x1, y1, x2, y2, duration)
  local arguments = string.format("swipe %s %s %s %s", x1, y1, x2, y2)

  if duration and duration ~= "" then
    arguments = arguments .. " " .. duration
  end

  return M.input(arguments)
end

function M.text(value)
  return M.input("text " .. value)
end

function M.uninstall(arguments)
  return M.pm("uninstall " .. arguments)
end

-- Comandos "fire-and-forget" (root, remount, reboot, tcpip) retornam
-- ok:boolean + mensagem, via out-parameter char**.
local function run_bool_cmd(title, fn)
  if not lib then
    return
  end
  local msg_ptr = ffi.new("char*[1]")
  local ok = fn(msg_ptr) == 1
  local msg = take_and_free(msg_ptr[0])
  M.show_output(title .. (ok and " (ok)" or " (falhou)"), msg or "")
  return ok, msg
end

function M.root()
  return run_bool_cmd("adb root", function(msg_ptr)
    return lib.adb_root(c_serial(), msg_ptr)
  end)
end

function M.remount()
  return run_bool_cmd("adb remount", function(msg_ptr)
    return lib.adb_remount(c_serial(), msg_ptr)
  end)
end

function M.reboot(mode)
  mode = mode or ""
  return run_bool_cmd("adb reboot " .. mode, function(msg_ptr)
    return lib.adb_reboot(c_serial(), mode, msg_ptr)
  end)
end

function M.tcpip(port)
  if not port then
    vim.notify(
      "[adb-bridge] AdbTcpip precisa de uma porta, ex: :AdbTcpip 5555",
      vim.log.levels.ERROR
    )
    return
  end
  return run_bool_cmd("adb tcpip " .. port, function(msg_ptr)
    return lib.adb_tcpip(c_serial(), port, msg_ptr)
  end)
end

-- Mostra texto num split horizontal scratch buffer.
function M.show_output(title, text)
  text = text or ""
  vim.cmd("botright split")
  vim.cmd("resize 15")
  local buf = vim.api.nvim_create_buf(false, true)
  vim.api.nvim_win_set_buf(0, buf)
  pcall(vim.api.nvim_buf_set_name, buf, title)
  vim.bo[buf].buftype = "nofile"
  vim.bo[buf].bufhidden = "wipe"

  local lines = {}
  for line in (text .. "\n"):gmatch("(.-)\n") do
    table.insert(lines, line)
  end
  vim.api.nvim_buf_set_lines(buf, 0, -1, false, lines)
end

return M
