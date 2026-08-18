// src/adb_transport.hpp
//
// Serviços que operam sobre um device específico, via
// host:transport:<serial> (ou host:transport-any se serial for vazio).

#pragma once

#include <string>

namespace adb::transport
{

// shell:<cmd> — equivalente a `adb -s <serial> shell <cmd>`.
std::string shell(const std::string &serial, const std::string &cmd);

// host-serial:<serial>:get-state (ou host:get-state se serial vazio).
std::string get_state(const std::string &serial);

// root: — reinicia o adbd como root no device (equivalente a `adb root`).
// Retorna true se o comando foi aceito (OKAY); a mensagem do adbd, se
// houver, vem em out_message.
bool root(const std::string &serial, std::string &out_message);

// unroot: — desliga o modo root (equivalente a `adb unroot`), voltando
// o adbd a rodar como usuário normal (shell).
bool unroot(const std::string &serial, std::string &out_message);

// remount: — remonta partições do sistema como read-write.
bool remount(const std::string &serial, std::string &out_message);

// reboot:<mode> — mode pode ser "", "bootloader" ou "recovery".
bool reboot(const std::string &serial, const std::string &mode, std::string &out_message);

// tcpip:<port> — coloca o device em modo adb-over-network na porta dada.
bool tcpip(const std::string &serial, int port, std::string &out_message);

// host[-serial:<serial>]:forward:<local>;<remote> — port forwarding,
// ex.: local_spec="tcp:8080", remote_spec="tcp:8080". Não suporta porta
// local dinâmica (tcp:0); use uma porta fixa.
bool forward(const std::string &serial, const std::string &local_spec,
             const std::string &remote_spec, std::string &out_message);

} // namespace adb::transport
