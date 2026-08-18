// src/adb_bridge_api.cpp
//
// Fachada extern "C" exposta via LuaJIT FFI. Fica fina de propósito: só
// converte std::string <-> char* e delega pra adb::host / adb::transport / adb::sync.

#include "adb_host.hpp"
#include "adb_sync.hpp"
#include "adb_transport.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
char *dup_cstr(const std::string &s)
{
    char *out = static_cast<char *>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

std::string safe_string(const char *value)
{
    return value ? std::string(value) : std::string();
}

int return_bool(bool ok, const std::string &message, char **out_message)
{
    if (out_message)
        *out_message = dup_cstr(message);
    return ok ? 1 : 0;
}
} // namespace

extern "C"
{
    // ---- host ----
    char *adb_devices(void) { return dup_cstr(adb::host::devices()); }
    char *adb_devices_l(void) { return dup_cstr(adb::host::devices_l()); }
    char *adb_version(void) { return dup_cstr(adb::host::version()); }

    // ---- transport ----
    char *adb_shell(const char *serial, const char *cmd)
    {
        return dup_cstr(adb::transport::shell(safe_string(serial), safe_string(cmd)));
    }

    char *adb_get_state(const char *serial)
    {
        return dup_cstr(adb::transport::get_state(safe_string(serial)));
    }

    int adb_root(const char *serial, char **out_message)
    {
        std::string msg;
        return return_bool(adb::transport::root(safe_string(serial), msg), msg, out_message);
    }

    int adb_unroot(const char *serial, char **out_message)
    {
        std::string msg;
        return return_bool(adb::transport::unroot(safe_string(serial), msg), msg, out_message);
    }

    int adb_remount(const char *serial, char **out_message)
    {
        std::string msg;
        return return_bool(adb::transport::remount(safe_string(serial), msg), msg, out_message);
    }

    int adb_reboot(const char *serial, const char *mode, char **out_message)
    {
        std::string msg;
        return return_bool(adb::transport::reboot(safe_string(serial), safe_string(mode), msg), msg,
                           out_message);
    }

    int adb_tcpip(const char *serial, int port, char **out_message)
    {
        std::string msg;
        return return_bool(adb::transport::tcpip(safe_string(serial), port, msg), msg, out_message);
    }

    int adb_forward(const char *serial, const char *local_spec, const char *remote_spec,
                    char **out_message)
    {
        std::string msg;
        return return_bool(adb::transport::forward(safe_string(serial), safe_string(local_spec),
                                                   safe_string(remote_spec), msg),
                           msg, out_message);
    }

    // ---- sync ----
    int adb_push(const char *serial, const char *local_path, const char *remote_path,
                 char **out_message)
    {
        std::string msg;
        return return_bool(adb::sync::push(safe_string(serial), safe_string(local_path),
                                           safe_string(remote_path), msg),
                           msg, out_message);
    }

    int adb_pull(const char *serial, const char *remote_path, const char *local_path,
                 char **out_message)
    {
        std::string msg;
        return return_bool(adb::sync::pull(safe_string(serial), safe_string(remote_path),
                                           safe_string(local_path), msg),
                           msg, out_message);
    }

    void adb_free(char *ptr) { std::free(ptr); }
}
