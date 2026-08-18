Sim, você pode adicionar muitos comandos ao plugin. Porém, as definições do Makefile dividem-se em dois grupos: comandos que usam `adb shell` e comandos que dependem de outros protocolos do ADB.

## 1. Comandos que seu plugin já consegue executar

Estas definições:

```make
INPUT       := $(ADB_SHELL) input
SETTINGS    := $(ADB_SHELL) settings
UIAUTOMATOR := $(ADB_SHELL) uiautomator
AM          := $(ADB_SHELL) am
PM          := $(ADB_SHELL) pm
START       := $(AM) start
```

são apenas comandos executados dentro do Android. Seu plugin já consegue usá-los com `:AdbShell`.

Exemplos:

```vim
:AdbShell input tap 500 800
:AdbShell input swipe 500 1200 500 300 400
:AdbShell input keyevent 4
:AdbShell input text hello
:AdbShell settings get system screen_brightness
:AdbShell am start -n com.example.app/.MainActivity
:AdbShell pm list packages
:AdbShell uiautomator dump /sdcard/window.xml
```

Portanto, tecnicamente você já possui os equivalentes de:

* `INPUT`
* `SETTINGS`
* `UIAUTOMATOR`
* `AM`
* `PM`
* `START`
* `TAP`
* `KEYEVENT`
* `SWIPE`
* `TEXT`

Criar comandos Neovim específicos apenas tornaria o uso mais conveniente.

## 2. Criando comandos especializados no Lua

Você poderia adicionar ao `setup()`:

```lua
vim.api.nvim_create_user_command("AdbTap", function(opts)
    M.shell("input tap " .. opts.args)
end, {
    nargs = "+",
    desc = "Simula um toque: AdbTap <x> <y>",
})

vim.api.nvim_create_user_command("AdbSwipe", function(opts)
    M.shell("input swipe " .. opts.args)
end, {
    nargs = "+",
    desc = "Simula gesto: AdbSwipe <x1> <y1> <x2> <y2> [duração]",
})

vim.api.nvim_create_user_command("AdbKeyevent", function(opts)
    M.shell("input keyevent " .. opts.args)
end, {
    nargs = "+",
    desc = "Envia uma tecla Android",
})

vim.api.nvim_create_user_command("AdbText", function(opts)
    M.shell("input text " .. opts.args)
end, {
    nargs = "+",
    desc = "Digita texto no dispositivo",
})

vim.api.nvim_create_user_command("AdbStart", function(opts)
    M.shell("am start " .. opts.args)
end, {
    nargs = "+",
    desc = "Inicia uma Activity",
})

vim.api.nvim_create_user_command("AdbPm", function(opts)
    M.shell("pm " .. opts.args)
end, {
    nargs = "+",
    desc = "Executa o Package Manager",
})

vim.api.nvim_create_user_command("AdbSettings", function(opts)
    M.shell("settings " .. opts.args)
end, {
    nargs = "+",
    desc = "Consulta ou altera configurações Android",
})

vim.api.nvim_create_user_command("AdbUiAutomator", function(opts)
    M.shell("uiautomator " .. opts.args)
end, {
    nargs = "+",
    desc = "Executa UI Automator",
})
```

O uso seria:

```vim
:AdbTap 500 800
:AdbSwipe 500 1200 500 300 400
:AdbKeyevent KEYCODE_HOME
:AdbText Ola
:AdbStart -n com.example.app/.MainActivity
:AdbPm list packages
:AdbSettings get system screen_brightness
:AdbUiAutomator dump /sdcard/window.xml
```

Não é necessário criar uma função C++ para cada um. Todos podem reutilizar:

```lua
M.shell("...")
```

e, portanto, o serviço existente:

```text
shell:<comando>
```

## 3. Criando funções Lua reutilizáveis

Além dos comandos do Neovim, você pode expor funções:

```lua
function M.tap(x, y)
    return M.shell(string.format(
        "input tap %d %d",
        x,
        y
    ))
end

function M.swipe(x1, y1, x2, y2, duration)
    local cmd = string.format(
        "input swipe %d %d %d %d",
        x1,
        y1,
        x2,
        y2
    )

    if duration then
        cmd = cmd .. " " .. tostring(duration)
    end

    return M.shell(cmd)
end

function M.keyevent(key)
    return M.shell("input keyevent " .. tostring(key))
end

function M.text(value)
    return M.shell("input text " .. value)
end

function M.start(component)
    return M.shell("am start -n " .. component)
end

function M.pm(arguments)
    return M.shell("pm " .. arguments)
end

function M.settings(arguments)
    return M.shell("settings " .. arguments)
end

function M.uiautomator(arguments)
    return M.shell("uiautomator " .. arguments)
end
```

Assim, outro arquivo Lua poderia usar:

```lua
local adb = require("adb-bridge")

adb.tap(500, 800)
adb.keyevent("KEYCODE_HOME")
adb.start("com.example.app/.MainActivity")
```

## 4. Comandos que exigem implementação adicional

Estas operações não são simples `shell:`:

```make
ADB_INSTALL  := $(ADB) install
ADB_PUSH     := $(ADB) push
ADB_PULL     := $(ADB) pull
ADB_EXEC_OUT := $(ADB) exec-out
```

### `push` e `pull`

Eles usam o subprotocolo binário `sync` do ADB. Seu próprio esqueleto já declara que isso não está implementado.

Para suportá-los diretamente, seria necessário implementar serviços como:

```text
sync:
SEND
DATA
DONE
RECV
STAT
```

Esse trabalho deve ficar na parte C++, pois envolve:

* cabeçalhos binários;
* tamanhos em little-endian;
* envio de arquivos em blocos;
* permissões e timestamps;
* leitura e gravação de arquivos locais;
* tratamento de respostas `OKAY` e `FAIL`.

Uma interface futura poderia ser:

```cpp
bool push(
    const std::string &serial,
    const std::string &local_path,
    const std::string &remote_path,
    std::string &message
);

bool pull(
    const std::string &serial,
    const std::string &remote_path,
    const std::string &local_path,
    std::string &message
);
```

E no Neovim:

```vim
:AdbPush ./app.apk /data/local/tmp/app.apk
:AdbPull /sdcard/window.xml ./window.xml
```

### `install`

`adb install` não é somente uma chamada direta a `pm install`.

O cliente normalmente precisa:

1. transferir o APK;
2. solicitar sua instalação;
3. acompanhar a resposta;
4. eventualmente instalar múltiplos APKs por sessão.

Depois de implementar `push`, uma primeira versão simples poderia:

```text
push app.apk → /data/local/tmp/app.apk
shell:pm install -r /data/local/tmp/app.apk
shell:rm /data/local/tmp/app.apk
```

Isso não cobre todos os recursos modernos de `adb install`, mas já seria funcional para um APK comum.

### `uninstall`

Esse é mais simples, pois pode ser realizado pelo `pm`:

```vim
:AdbShell pm uninstall com.example.app
```

Você poderia criar:

```lua
function M.uninstall(package_name)
    return M.shell("pm uninstall " .. package_name)
end
```

E o comando:

```lua
vim.api.nvim_create_user_command("AdbUninstall", function(opts)
    M.uninstall(opts.args)
end, {
    nargs = 1,
    desc = "Desinstala um pacote Android",
})
```

Uso:

```vim
:AdbUninstall com.example.app
```

Isso fornece o efeito principal de `adb uninstall`, embora opções especiais precisem ser tratadas.

### `exec-out`

`adb exec-out` é usado principalmente quando você precisa preservar a saída bruta, sem terminal interativo e sem alterações no conteúdo.

Exemplos conhecidos:

```bash
adb exec-out screencap -p > tela.png
adb exec-out screenrecord --output-format=h264 - > video.h264
```

A função `M.shell()` atual devolve uma string Lua, mas `show_output()` a trata como texto e a coloca em um buffer. Isso não funciona corretamente para PNG, vídeo ou outros dados binários.

O ideal seria criar uma API que grave diretamente a resposta em arquivo:

```vim
:AdbExecOut tela.png screencap -p
```

Conceitualmente:

```cpp
bool exec_out_to_file(
    const std::string &serial,
    const std::string &command,
    const std::string &local_path,
    std::string &message
);
```

Ela receberia o stream e escreveria os bytes sem convertê-los em texto.

## Organização recomendada

Eu dividiria seu plugin assim:

| Grupo         | Exemplos                                       | Implementação                    |
| ------------- | ---------------------------------------------- | -------------------------------- |
| Shell         | `input`, `am`, `pm`, `settings`                | Wrappers Lua sobre `M.shell()`   |
| Controle ADB  | `root`, `unroot`, `reboot`, `remount`, `tcpip` | Serviços C++ próprios            |
| Arquivos      | `push`, `pull`, `stat`                         | Subprotocolo `sync` em C++       |
| Saída binária | captura de tela e vídeo                        | Stream C++ gravado em arquivo    |
| Instalação    | `install`, `install-multiple`                  | `sync` + protocolo de instalação |
| Monitoramento | `logcat`, `track-devices`                      | Operação assíncrona com libuv    |

## Outros comandos úteis que você pode adicionar

Usando apenas o `M.shell()` atual:

```vim
:AdbShell wm size
:AdbShell wm density
:AdbShell dumpsys battery
:AdbShell dumpsys activity activities
:AdbShell dumpsys window
:AdbShell getprop
:AdbShell logcat -d
:AdbShell screencap -p /sdcard/tela.png
:AdbShell screenrecord /sdcard/video.mp4
:AdbShell cmd package list packages
:AdbShell am force-stop com.example.app
:AdbShell monkey -p com.example.app 1
```

Os melhores candidatos para comandos dedicados seriam:

```text
:AdbTap
:AdbSwipe
:AdbText
:AdbKeyevent
:AdbStart
:AdbStop
:AdbInstall
:AdbUninstall
:AdbPush
:AdbPull
:AdbScreenshot
:AdbLogcat
:AdbUnroot
```

Minha recomendação é começar pelos wrappers de `input`, `am`, `pm` e `settings`, pois eles exigem somente Lua. Depois, implementar `unroot:` em C++, e deixar `push`, `pull`, `install` e captura binária para a próxima etapa estrutural.

