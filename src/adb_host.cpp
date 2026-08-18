// src/adb_host.cpp

#include "adb_host.hpp"
#include "adb_connection.hpp"

namespace adb::host
{

namespace
{
// Helper comum aos três serviços: conecta, envia o comando host:X e
// devolve o corpo já com framing decodificado.
std::string simple_host_request(const std::string &command)
{
    Connection conn;
    if (!conn.valid())
        return "ERROR: nao foi possivel conectar ao adb server (rode: adb start-server)";

    std::string err;
    if (!conn.send_request(command) || conn.read_status(err) != Status::Okay)
        return "ERROR: " + (err.empty() ? std::string("comando falhou") : err);

    return conn.read_framed();
}
} // namespace

std::string devices()
{
    return simple_host_request("host:devices");
}

std::string devices_l()
{
    return simple_host_request("host:devices-l");
}

std::string version()
{
    return simple_host_request("host:version");
}

} // namespace adb::host
