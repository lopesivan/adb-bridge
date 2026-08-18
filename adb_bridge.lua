return {
    {
        "lopesivan/adb-bridge", -- ajuste para o seu repo quando publicar

        cmd = {
            "AdbShell",
            "AdbDevices",
        },

        build = "make",

        opts = {
            lib_path = nil, -- nil = busca automática
            serial   = nil, -- nil = usa único device conectado
        },

        config = function(_, opts)
            require("adb-bridge").setup(opts)
        end,
    },
}
