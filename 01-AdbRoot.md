Ao executar `:AdbRoot`, **não é o Android inteiro que passa a rodar como root**. O que acontece é mais específico: o processo **`adbd`**, responsável pelas conexões ADB no dispositivo, é reiniciado com privilégios de root.

O fluxo do projeto é este:

```text
:AdbRoot
    ↓
M.root() em Lua
    ↓
adb_root() na API C
    ↓
adb::transport::root()
    ↓
envia "root:" ao adb server
    ↓
o adbd do Android reinicia como root
```

No seu `adb_transport.cpp`, isso aparece diretamente:

```cpp
bool root(const std::string &serial, std::string &out_message)
{
    return transport_command(serial, "root:", out_message);
}
```

## O que muda depois do `AdbRoot`

Antes, um comando como:

```vim
:AdbShell id
```

normalmente executa dentro do dispositivo como o usuário `shell`:

```text
uid=2000(shell) gid=2000(shell) ...
```

Depois do `:AdbRoot`, uma nova execução de:

```vim
:AdbShell id
```

pode retornar:

```text
uid=0(root) gid=0(root) ...
```

Isso acontece porque `AdbShell` também passa pelo `adbd`. Como agora o daemon está rodando como root, os shells abertos por ele herdam esses privilégios.

Consequentemente, seus comandos ADB podem acessar muito mais coisas:

```vim
:AdbShell ls -la /data
:AdbShell cat /data/system/packages.xml
:AdbShell id
```

E o comando:

```vim
:AdbRemount
```

pode ter permissão para tentar remontar partições do sistema como graváveis.

## O que não muda

Os outros processos do Android não são automaticamente convertidos em root:

* os aplicativos continuam usando seus próprios UIDs;
* o aplicativo de câmera não vira root;
* o launcher não vira root;
* os serviços existentes não mudam automaticamente de usuário;
* o Neovim no computador não vira root;
* o `adb server` no computador também não vira root.

Portanto, o resultado correto é:

> O canal ADB passa a oferecer um shell privilegiado porque o `adbd` foi reiniciado como root.

Não significa que todo o Android passou a executar seus processos como root.

## Por que parece que “o sistema todo” virou root?

Porque todo comando executado posteriormente pelo seu plugin passa pelo mesmo `adbd` privilegiado:

```vim
:AdbShell whoami
:AdbShell id
:AdbShell ls /data
```

Como todos retornam ou atuam como root, dá a impressão de que o sistema inteiro mudou. Na realidade, é o seu ponto de entrada administrativo — o ADB — que ficou privilegiado.

## O efeito vale para o ADB inteiro

O privilégio não pertence somente ao plugin. Seu plugin envia a mesma solicitação que:

```bash
adb root
```

Depois disso, uma chamada feita pelo `adb` convencional também deverá obter um shell privilegiado:

```bash
adb shell id
```

Isso ocorre porque o estado está no `adbd` do dispositivo, não no Neovim nem na biblioteca `adb_bridge.so`.

## Reinício e limitações

Em geral, o estado pode durar até:

* reiniciar o dispositivo;
* reiniciar o `adbd` sem privilégios;
* executar `adb unroot`, se a implementação do sistema aceitar.

Seu plugin atualmente implementa `root:`, mas não implementa `unroot:`. Poderia ser adicionado um `:AdbUnroot` que envie:

```text
unroot:
```

Além disso, `adb root` normalmente só funciona em:

* builds Android `userdebug`;
* builds `eng`;
* ROMs personalizadas que permitem isso;
* emuladores compatíveis.

Em builds comerciais `user`, o resultado normal é algo como:

```text
adbd cannot run as root in production builds
```

Mesmo com `adbd` como root, SELinux ainda pode impor restrições. `uid=0` não significa necessariamente acesso absolutamente irrestrito a todos os recursos do Android.

