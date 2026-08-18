# adb-bridge

Plugin para Neovim que fala diretamente com o `adb server` local
(`127.0.0.1:5037`) via protocolo host do ADB, usando LuaJIT FFI. Evita
fork+exec do binário `adb` a cada chamada. Implementado em C++17.

## Requisitos

- `g++` com suporte a C++17
- `adb server` rodando (`adb start-server` — geralmente já está, se você
  usa o SDK do Android normalmente)

## Estrutura do projeto

```
src/
├── adb_connection.hpp/.cpp  -- RAII: socket + framing do protocolo (base)
├── adb_host.hpp/.cpp        -- serviços "host:" (devices, devices-l, version)
├── adb_transport.hpp/.cpp   -- serviços por-device (shell, root, remount, reboot, tcpip)
└── adb_bridge_api.cpp       -- fachada extern "C", único arquivo visível pra FFI
```

Pra adicionar um serviço novo do protocolo adb:
1. Função nova em `adb_host.cpp` ou `adb_transport.cpp` (conforme o caso)
2. Wrapper `extern "C"` em `adb_bridge_api.cpp` (converte `std::string` → `char*`)
3. Entrada no `ffi.cdef` de `lua/adb-bridge/init.lua`
4. Função Lua + `:UserCommand` correspondente

A classe `adb::Connection` (em `adb_connection.hpp`) nunca precisa mudar —
ela só implementa o framing do protocolo, não os comandos específicos.

## Build

```
make
```

Gera `adb_bridge.so` na raiz do projeto.

## Uso

```vim
:AdbShell ls
:AdbShell ls -la /sdcard
:AdbDevices
:AdbDevicesL
:AdbState
:AdbRoot
:AdbRemount
:AdbReboot
:AdbReboot bootloader
:AdbTcpip 5555
```

Cada comando abre um split horizontal scratch com a saída.

Via Lua:

```lua
local adb = require("adb-bridge")
local out = adb.shell("ls /sdcard")
print(out)
```

## Como funciona

1. Lua chama `adb_shell(serial, cmd)` via FFI
2. C abre socket TCP para `127.0.0.1:5037` (o daemon `adb server`)
3. Envia `host:transport-any` (ou `host:transport:<serial>`) seguido de
   `shell:<cmd>`, no formato do protocolo host (`%04x` + payload ASCII)
4. Lê a resposta como stream bruto até o servidor fechar a conexão
5. Retorna a string pro Lua; `adb_free` libera a memória alocada em C

## Limitações do esqueleto (próximos passos)

- `serial = nil` usa `host:transport-any`, que só funciona com **um**
  device conectado. Com múltiplos devices, defina `opts.serial` (você
  pode obter os seriais com `:AdbDevices`).
- Não implementa o subprotocolo `sync` (usado por `adb push`/`adb pull`),
  que é binário e mais complexo que `shell:`/`host:devices`. Por ora, use
  o `adb` normal pra isso.
- Sem timeout de socket — se o `adb server` não responder, a chamada pode
  travar o Neovim (bloqueante). Vale rodar via `vim.uv`/libuv assíncrono
  numa próxima iteração, em vez de FFI síncrono direto na main thread.
- Sem tratamento de auth USB (isso é responsabilidade do `adb server`,
  que já lida com isso antes de você conectar no socket local).

## Licença

MIT
