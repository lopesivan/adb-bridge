-- lua/adb-bridge/init.lua
-- Bridge nativo para adb server via LuaJIT FFI (sem fork+exec do binário adb).

local M = {}
local ffi = require("ffi")

ffi.cdef([[
    char *adb_shell(const char *serial, const char *cmd);
    char *adb_devices(void);
    void  adb_free(char *ptr);
]])

local lib = nil
local config = {
    lib_path = nil, -- nil = busca automática ao lado deste init.lua
    serial   = nil, -- nil = usa o único device conectado (host:transport-any)
}

local function find_lib_path()
    local src = debug.getinfo(1, "S").source:sub(2)
    local plugin_root = src:match("(.*/)lua/adb%-bridge/init%.lua$")
    if plugin_root then
        return plugin_root .. "adb_bridge.so"
    end
    return nil
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
        vim.notify("[adb-bridge] falha ao carregar " .. lib_path .. ": " .. tostring(result), vim.log.levels.ERROR)
        return
    end
    lib = result

    vim.api.nvim_create_user_command("AdbShell", function(cmdopts)
        M.shell(cmdopts.args)
    end, { nargs = "+", desc = "Executa `adb shell <cmd>` via bridge nativo" })

    vim.api.nvim_create_user_command("AdbDevices", function()
        M.devices()
    end, { desc = "Lista dispositivos (equivalente a `adb devices`)" })
end

-- Executa `adb shell <cmd>` e mostra o resultado num split scratch.
-- Retorna a saída como string.
function M.shell(cmd)
    if not lib then
        vim.notify("[adb-bridge] setup() não foi chamado", vim.log.levels.ERROR)
        return
    end

    local result_ptr = lib.adb_shell(config.serial, cmd)
    local result = ffi.string(result_ptr)
    lib.adb_free(result_ptr)

    M.show_output("adb shell " .. cmd, result)
    return result
end

-- Executa `adb devices` e mostra o resultado num split scratch.
function M.devices()
    if not lib then
        vim.notify("[adb-bridge] setup() não foi chamado", vim.log.levels.ERROR)
        return
    end

    local result_ptr = lib.adb_devices()
    local result = ffi.string(result_ptr)
    lib.adb_free(result_ptr)

    M.show_output("adb devices", result)
    return result
end

-- Mostra texto num split horizontal scratch buffer (fecha sozinho ao sair).
function M.show_output(title, text)
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
