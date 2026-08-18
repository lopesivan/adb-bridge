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

Com a expansão atual, os comandos seguem a sintaxe do adb tradicional, mas são executados pelo plugin dentro do Neovim.

AdbPull

Formato:

:AdbPull <arquivo-remoto> <arquivo-local>

Por exemplo, para copiar do Android:

/sdcard/Download/teste.txt

para o diretório atual:

:AdbPull /sdcard/Download/teste.txt ./teste.txt

É equivalente a:

adb pull /sdcard/Download/teste.txt ./teste.txt

Internamente o caminho é:

:AdbPull
   ↓
M.pull()
   ↓ FFI
adb_pull()
   ↓
adb::sync::pull()
   ↓
sync:
   ↓
RECV
   ↓
DATA ...
   ↓
DONE

AdbPush

É o inverso:

:AdbPush <arquivo-local> <arquivo-remoto>

Exemplo:

:AdbPush ./teste.txt /sdcard/Download/teste.txt

equivalente a:

adb push ./teste.txt /sdcard/Download/teste.txt

Aqui o protocolo sync: envia aproximadamente:

SEND
 ↓
DATA
 ↓
DATA
 ↓
...
 ↓
DONE
 ↓
OKAY

AdbInstall

Para instalar um APK:

:AdbInstall ./app-debug.apk

ou, com caminho absoluto:

:AdbInstall /workspace/MyApp/app/build/outputs/apk/debug/app-debug.apk

Nossa implementação faz:

AdbInstall app-debug.apk
        │
        ├── AdbPush
        │      ↓
        │ /data/local/tmp/adb-bridge-*.apk
        │
        ├── shell pm install -r ...
        │
        └── rm do APK temporário

Portanto ele já aproveita o push nativo que acabamos de conectar ao Lua.

AdbForward

Para encaminhar uma porta do computador para o Android:

:AdbForward tcp:8080 tcp:8080

equivale a:

adb forward tcp:8080 tcp:8080

Por exemplo, se existe um serviço no Android escutando 8080:

PC                         Android

localhost:8080
      │
      │ ADB forward
      └──────────────────► localhost:8080
                            no device

Você acessa no computador:

curl localhost:8080

AdbUnroot

Agora também existe:

:AdbUnroot

equivalente a:

adb unroot

Enquanto:

:AdbRoot

faz o contrário.

AdbVersion

Também expus o host:version que já existia no C++:

:AdbVersion

Ele consulta diretamente o servidor ADB, sem precisar de um dispositivo.


---

Você também pode chamar tudo diretamente em Lua, o que é particularmente útil para seus mappings e scripts:

local adb = require("adb-bridge")

adb.pull(
  "/sdcard/Download/teste.txt",
  "./teste.txt"
)

adb.push(
  "./teste.txt",
  "/sdcard/Download/teste.txt"
)

adb.install("./app-debug.apk")

adb.forward("tcp:8080", "tcp:8080")

adb.unroot()

local version = adb.version()

E os comandos antigos continuam funcionando normalmente, por exemplo:

:AdbDevices
:AdbDevicesL
:AdbState
:AdbShell ls -la /sdcard
:AdbRoot
:AdbUnroot
:AdbRemount
:AdbReboot
:AdbReboot bootloader
:AdbTcpip 5555
:AdbTap 500 800
:AdbSwipe 500 1200 500 300 400
:AdbKeyevent KEYCODE_HOME
:AdbText hello
:AdbPm list packages
:AdbUninstall com.example.app

Para testar a expansão, eu começaria por AdbPush + AdbPull, porque são justamente a parte nova mais importante. Crie um arquivo pequeno no PC, envie para /sdcard/Download, confira com AdbShell cat, apague o original local e faça AdbPull de volta. Assim validamos todo o novo sync: de ponta a ponta.


Sim. Toda a comunicação nativa do seu adb-bridge com o ADB server é feita por socket TCP.

A arquitetura é:

Neovim
   │
   │ LuaJIT FFI
   ▼
adb_bridge.so
   │
   │ socket TCP
   ▼
127.0.0.1:5037
   │
   ▼
ADB Server
   │
   │ USB ou TCP/IP
   ▼
Android

Onde o socket é criado

No seu adb_connection.cpp:

int fd = socket(AF_INET, SOCK_STREAM, 0);

Isso significa:

AF_INET      → IPv4
SOCK_STREAM  → TCP

Depois:

addr.sin_port = htons(5037);
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

e finalmente:

connect(
    fd,
    reinterpret_cast<struct sockaddr *>(&addr),
    sizeof(addr)
);

Portanto a .so estabelece uma conexão:

127.0.0.1:<porta efêmera> ──TCP──► 127.0.0.1:5037
                                      ADB server

Inclusive AdbPush e AdbPull

Sim. Eles também passam pelo mesmo socket TCP.

A diferença é o protocolo usado dentro da conexão.

Para um shell:

TCP socket
    │
    ├── "host:transport:<serial>"
    │
    ├── OKAY
    │
    ├── "shell:ls /sdcard"
    │
    ├── OKAY
    │
    └── stream da resposta

Para AdbPull:

TCP socket
    │
    ├── host:transport:<serial>
    ├── OKAY
    │
    ├── sync:
    ├── OKAY
    │
    ├── RECV
    ├── DATA
    ├── DATA
    ├── DATA
    └── DONE

E AdbPush:

TCP socket
    │
    ├── host:transport:<serial>
    ├── OKAY
    │
    ├── sync:
    ├── OKAY
    │
    ├── SEND
    ├── DATA
    ├── DATA
    ├── DONE
    └── OKAY

Ou seja, não usamos o executável adb para essas operações. O seu C++ está implementando diretamente partes do protocolo que o próprio cliente adb normalmente utilizaria para conversar com o servidor.

Um detalhe importante: o socket TCP que implementamos é PC → ADB server local. A comunicação seguinte, ADB server → Android, pode ser USB ou TCP/IP. Essa segunda parte é responsabilidade do ADB server, não do seu plugin.

