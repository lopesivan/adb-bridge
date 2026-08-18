Atualizei o arquivo usando apenas o `M.shell()` existente, sem implementar nada em C++.

Foram adicionados os comandos:

```vim
:AdbInput
:AdbSettings
:AdbUiAutomator
:AdbAm
:AdbPm
:AdbStart
:AdbTap
:AdbKeyevent
:AdbSwipe
:AdbText
:AdbUninstall
```

E as funções Lua exportadas:

```lua
adb.input()
adb.settings()
adb.uiautomator()
adb.am()
adb.pm()
adb.start()
adb.tap()
adb.keyevent()
adb.swipe()
adb.text()
adb.uninstall()
```

Exemplos:

```vim
:AdbTap 500 800
:AdbSwipe 500 1200 500 300 400
:AdbKeyevent KEYCODE_HOME
:AdbText Ola
:AdbStart -n com.example.app/.MainActivity
:AdbPm list packages
:AdbSettings get system screen_brightness
:AdbUiAutomator dump /sdcard/window.xml
:AdbUninstall com.example.app
```

Não adicionei `install`, `push`, `pull` e `exec-out`, porque eles precisariam de suporte adicional correto na camada C++.

Arquivo atualizado: [](sandbox:/workspace/scratch/006221ef8fa0/upload/init%281%29.lua)

