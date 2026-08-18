return {
    {
        "lopesivan/adb-bridge",
        cmd    = {
            "AdbShell",
            "AdbDevices",
            "AdbDevicesL",
            "AdbState",
            "AdbRoot",
            "AdbRemount",
            "AdbReboot",
            "AdbTcpip"
        },

        build  = "make",

        opts   = {
            lib_path = nil, -- nil = busca automática
            serial   = nil, -- nil = usa único device conectado
        },

        config = function(_, opts)
            require("adb-bridge").setup(opts)
        end,
    },
}
