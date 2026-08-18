// src/adb_transport.cpp

#include "adb_transport.hpp"
#include "adb_connection.hpp"

namespace adb::transport
{

namespace
{
std::string transport_request(const std::string &serial)
{
    return serial.empty() ? std::string("host:transport-any")
                           : std::string("host:transport:") + serial;
}

// Padrão comum: host:transport:<serial> (OKAY) seguido de um comando
// "fire-and-forget" (root:, remount:, reboot:...) cujo corpo (se houver)
// vem como stream até fechar a conexão, igual ao shell:.
bool transport_command(const std::string &serial, const std::string &command,
                        std::string &out_message)
{
    Connection conn;
    if (!conn.valid())
    {
        out_message = "nao foi possivel conectar ao adb server";
        return false;
    }

    std::string err;
    if (!conn.send_request(transport_request(serial)) || conn.read_status(err) != Status::Okay)
    {
        out_message = "transport falhou: " + (err.empty() ? std::string("desconhecido") : err);
        return false;
    }

    if (!conn.send_request(command) || conn.read_status(err) != Status::Okay)
    {
        out_message = command + " falhou: " + (err.empty() ? std::string("desconhecido") : err);
        return false;
    }

    out_message = conn.read_until_eof();
    return true;
}
} // namespace

std::string shell(const std::string &serial, const std::string &cmd)
{
    std::string out;
    if (!transport_command(serial, "shell:" + cmd, out))
        return "ERROR: " + out;
    return out;
}

std::string get_state(const std::string &serial)
{
    Connection conn;
    if (!conn.valid())
        return "ERROR: nao foi possivel conectar ao adb server";

    std::string cmd = serial.empty() ? "host:get-state" : ("host-serial:" + serial + ":get-state");

    std::string err;
    if (!conn.send_request(cmd) || conn.read_status(err) != Status::Okay)
        return "ERROR: " + (err.empty() ? std::string("comando falhou") : err);

    return conn.read_framed();
}

bool root(const std::string &serial, std::string &out_message)
{
    return transport_command(serial, "root:", out_message);
}

bool remount(const std::string &serial, std::string &out_message)
{
    return transport_command(serial, "remount:", out_message);
}

bool reboot(const std::string &serial, const std::string &mode, std::string &out_message)
{
    return transport_command(serial, "reboot:" + mode, out_message);
}

bool tcpip(const std::string &serial, int port, std::string &out_message)
{
    return transport_command(serial, "tcpip:" + std::to_string(port), out_message);
}

} // namespace adb::transport
