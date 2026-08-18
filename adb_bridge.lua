return {
  {
    "lopesivan/adb-bridge",
    branch = "cpp",

    cmd = {
      "AdbShell",
      "AdbDevices",
      "AdbDevicesL",
      "AdbVersion",
      "AdbState",
      "AdbRoot",
      "AdbUnroot",
      "AdbRemount",
      "AdbReboot",
      "AdbTcpip",
      "AdbForward",
      "AdbPush",
      "AdbPull",
      "AdbInstall",
      "AdbInput",
      "AdbTap",
      "AdbSwipe",
      "AdbKeyevent",
      "AdbText",
      "AdbAm",
      "AdbStart",
      "AdbPm",
      "AdbSettings",
      "AdbUiAutomator",
      "AdbUninstall",
    },

    build = "make",

    opts = {
      lib_path = nil, -- nil = busca adb_bridge.so na raiz do plugin
      serial = nil, -- nil = host:transport-any
    },

    config = function(_, opts)
      require("adb-bridge").setup(opts)
    end,
  },
}
