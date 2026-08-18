# adb-bridge

Plugin para Neovim que fala diretamente com o `adb server` local
(`127.0.0.1:5037`) via protocolo ADB, usando LuaJIT FFI. Evita `fork+exec`
do binário `adb` a cada chamada. Implementado em C++17.

## Requisitos

- `g++` com suporte a C++17
- `adb server` rodando (`adb start-server`)

## Estrutura

```text
src/
├── adb_connection.hpp/.cpp  -- RAII do socket, framing host e I/O binário
├── adb_host.hpp/.cpp        -- serviços host: (devices, devices-l, version)
├── adb_transport.hpp/.cpp   -- shell, state, root/unroot, remount, reboot, tcpip, forward
├── adb_sync.hpp/.cpp        -- subprotocolo binário sync: para push/pull
└── adb_bridge_api.cpp       -- fachada extern "C" consumida pelo LuaJIT FFI
```

A regra de extensão é:

1. implementar o serviço no namespace C++ apropriado;
2. exportar um wrapper `extern "C"` em `adb_bridge_api.cpp` quando Lua precisar chamá-lo;
3. declarar o símbolo no `ffi.cdef`;
4. expor a função Lua e, quando útil, um `:UserCommand`.

Wrappers como `input`, `am`, `pm`, `settings`, `tap` e `swipe` reutilizam
`adb_shell()` e não precisam de novos símbolos C++.

## Build

```sh
make
```

Gera `adb_bridge.so` na raiz do plugin.

## Comandos Neovim

```vim
:AdbDevices
:AdbDevicesL
:AdbVersion
:AdbState
:AdbShell ls -la /sdcard

:AdbRoot
:AdbUnroot
:AdbRemount
:AdbReboot
:AdbReboot bootloader
:AdbTcpip 5555
:AdbForward tcp:8080 tcp:8080

:AdbPush ./arquivo.txt /sdcard/arquivo.txt
:AdbPull /sdcard/arquivo.txt ./arquivo.txt
:AdbInstall ./app.apk

:AdbInput keyevent KEYCODE_HOME
:AdbTap 500 800
:AdbSwipe 500 1200 500 300 400
:AdbKeyevent KEYCODE_HOME
:AdbText Ola

:AdbAm force-stop com.example.app
:AdbStart -n com.example.app/.MainActivity
:AdbPm list packages
:AdbSettings get system screen_brightness
:AdbUiAutomator dump /sdcard/window.xml
:AdbUninstall com.example.app
```

## API Lua

```lua
local adb = require("adb-bridge")

adb.devices()
adb.devices_l()
adb.version()
adb.get_state()
adb.shell("getprop ro.product.model")

adb.root()
adb.unroot()
adb.remount()
adb.reboot("bootloader")
adb.tcpip(5555)
adb.forward("tcp:8080", "tcp:8080")

adb.push("./arquivo.txt", "/sdcard/arquivo.txt")
adb.pull("/sdcard/arquivo.txt", "./arquivo.txt")
adb.install("./app.apk")

adb.tap(500, 800)
adb.swipe(500, 1200, 500, 300, 400)
adb.keyevent("KEYCODE_HOME")
adb.text("Ola")
```

## Como funciona

Para serviços textuais, Lua chama a fachada C, que abre um socket TCP para o
ADB server, envia o framing `%04x` + payload e interpreta `OKAY`/`FAIL`.
Operações por device selecionam `host:transport:<serial>` ou
`host:transport-any` antes do serviço (`shell:`, `root:`, etc.).

`push` e `pull` entram no serviço `sync:`. A partir daí a conexão usa pacotes
binários com IDs como `SEND`, `DATA`, `DONE`, `RECV`, `OKAY` e `FAIL`, e
comprimentos little-endian de 32 bits. Essa camada fica isolada em
`adb_sync.cpp`.

`AdbInstall` é uma conveniência Lua: faz `push` do APK para
`/data/local/tmp`, executa `pm install -r` e remove o arquivo temporário.
Ela cobre instalação simples de um APK; não pretende substituir ainda todos
os modos modernos de `adb install`/`install-multiple`.

## Limitações atuais

- `serial = nil` usa `host:transport-any`; com vários devices configure
  `opts.serial`.
- chamadas FFI são síncronas e podem bloquear a main thread do Neovim;
  ainda não há timeout de socket.
- `push`/`pull` implementam transferência de arquivo simples, não toda a
  superfície do protocolo sync (por exemplo, diretórios/STAT avançado).
- `forward` usa especificações explícitas como `tcp:8080`; porta local
  dinâmica `tcp:0` ainda não é tratada.
- `install` cobre APK único por `push + pm install -r`; não cobre sessões de
  instalação múltipla/streaming.
- autenticação USB continua sendo responsabilidade do `adb server`.

## Licença

MIT
