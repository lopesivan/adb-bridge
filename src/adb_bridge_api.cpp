// src/adb_bridge_api.cpp
//
// Fachada extern "C" exposta via LuaJIT FFI. Fica fina de propósito: só
// converte std::string <-> char* e delega pra adb::host / adb::transport.
// Adicionar uma função nova aqui = 1 linha no ffi.cdef do init.lua.

#include "adb_host.hpp"
#include "adb_transport.hpp"

#include <cstring>

namespace
{
// strdup manual (evita depender de _POSIX_C_SOURCE em todo o projeto).
char *dup_cstr(const std::string &s)
{
    char *out = static_cast<char *>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

// serial em C pode ser nullptr; convertemos pra std::string vazia.
std::string safe_serial(const char *serial)
{
    return serial ? std::string(serial) : std::string();
}
} // namespace

extern "C"
{
    // ---- host: ----

    char *adb_devices(void)
    {
        return dup_cstr(adb::host::devices());
    }

    char *adb_devices_l(void)
    {
        return dup_cstr(adb::host::devices_l());
    }

    char *adb_version(void)
    {
        return dup_cstr(adb::host::version());
    }

    // ---- transport (por device) ----

    char *adb_shell(const char *serial, const char *cmd)
    {
        return dup_cstr(adb::transport::shell(safe_serial(serial), cmd ? cmd : ""));
    }

    char *adb_get_state(const char *serial)
    {
        return dup_cstr(adb::transport::get_state(safe_serial(serial)));
    }

    // Retorna 1 em sucesso, 0 em falha. A mensagem (saída do adbd ou erro)
    // vai sempre em *out_message (malloc'd, chamador libera com adb_free).
    int adb_root(const char *serial, char **out_message)
    {
        std::string msg;
        bool ok = adb::transport::root(safe_serial(serial), msg);
        if (out_message)
            *out_message = dup_cstr(msg);
        return ok ? 1 : 0;
    }

    int adb_remount(const char *serial, char **out_message)
    {
        std::string msg;
        bool ok = adb::transport::remount(safe_serial(serial), msg);
        if (out_message)
            *out_message = dup_cstr(msg);
        return ok ? 1 : 0;
    }

    // mode: "" (normal), "bootloader" ou "recovery".
    int adb_reboot(const char *serial, const char *mode, char **out_message)
    {
        std::string msg;
        bool ok = adb::transport::reboot(safe_serial(serial), mode ? mode : "", msg);
        if (out_message)
            *out_message = dup_cstr(msg);
        return ok ? 1 : 0;
    }

    int adb_tcpip(const char *serial, int port, char **out_message)
    {
        std::string msg;
        bool ok = adb::transport::tcpip(safe_serial(serial), port, msg);
        if (out_message)
            *out_message = dup_cstr(msg);
        return ok ? 1 : 0;
    }

    // ---- utilitário ----

    void adb_free(char *ptr)
    {
        std::free(ptr);
    }
}
