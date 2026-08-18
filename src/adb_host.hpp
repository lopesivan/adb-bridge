// src/adb_host.hpp
//
// Serviços "host:" do protocolo adb — não dependem de um device
// específico (ou operam sobre o conjunto de devices conectados).

#pragma once

#include <string>

namespace adb::host
{

// host:devices — equivalente a `adb devices`.
std::string devices();

// host:devices-l — como devices(), mas com modelo/produto/transport_id.
std::string devices_l();

// host:version — versão do protocolo do adb server.
std::string version();

// Ponto de extensão futuro: host:track-devices fica escutando eventos de
// connect/disconnect. Diferente das demais funções aqui, essa não
// retorna uma vez só — precisa de uma conexão persistente + callback
// (ou polling assíncrono do lado Lua via vim.uv). Implementar quando for
// necessário; por ora, use devices()/devices_l() sob demanda.

} // namespace adb::host
