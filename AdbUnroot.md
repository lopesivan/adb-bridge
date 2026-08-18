Para voltar ao normal, use no terminal:

```bash
adb unroot
```

Isso reinicia o `adbd` como o usuário comum `shell`. Depois, confirme:

```bash
adb shell id
```

O resultado deve começar aproximadamente assim:

```text
uid=2000(shell) gid=2000(shell)
```

Como seu plugin ainda não possui `:AdbUnroot`, você também pode simplesmente reiniciar o aparelho:

```vim
:AdbReboot
```

Após o Android iniciar novamente, normalmente o `adbd` volta ao modo padrão.

Importante: sair do Neovim ou desconectar o cabo USB não desfaz necessariamente o `adb root`, pois o estado pertence ao `adbd` no dispositivo.

Para adicionar futuramente ao plugin, o comando deverá enviar:

```text
unroot:
```

de maneira equivalente ao `root:` já implementado. A interface poderia ser:

```vim
:AdbUnroot
```

e internamente:

```cpp
bool unroot(const std::string &serial, std::string &out_message)
{
    return transport_command(serial, "unroot:", out_message);
}
```

