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
            "AdbTap",
            "AdbSwipe",
            "AdbKeyevent",
            "AdbText",
            "AdbStart",
            "AdbPm",
            "AdbSettings",
            "AdbUiAutomator",
            "AdbUninstall",
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
