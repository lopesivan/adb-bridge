// src/adb_sync.hpp
//
// Subprotocolo "sync:" do adb — usado por push/pull. Diferente de
// host:/shell:, esse protocolo é BINÁRIO: cada pacote é um id de 4 bytes
// ASCII ("SEND", "DATA", "DONE", "RECV", "OKAY", "FAIL") seguido de um
// tamanho em 4 bytes little-endian (não hex-ASCII) e, quando aplicável,
// o payload correspondente.

#pragma once

#include <string>

namespace adb::sync
{

// Envia um arquivo local pro device (equivalente a `adb push local remote`).
// Preserva o modo (permissões) e mtime do arquivo local.
bool push(const std::string &serial, const std::string &local_path,
          const std::string &remote_path, std::string &out_message);

// Copia um arquivo do device pro host (equivalente a `adb pull remote local`).
bool pull(const std::string &serial, const std::string &remote_path,
          const std::string &local_path, std::string &out_message);

} // namespace adb::sync
