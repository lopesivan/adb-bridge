// src/adb_connection.hpp
//
// Conexão RAII com o adb server local (127.0.0.1:5037) e as primitivas
// de framing do protocolo host. Toda comunicação com o adb server passa
// por aqui — os arquivos adb_host.cpp e adb_transport.cpp usam essa base
// pra implementar serviços específicos.

#pragma once

#include <string>

namespace adb
{

// Resultado de leitura de status OKAY/FAIL.
enum class Status
{
    Okay,
    Fail,
    IoError,
};

class Connection
{
public:
    Connection();
    ~Connection();

    // Não copiável (dono único do fd), movível.
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    Connection(Connection &&other) noexcept;
    Connection &operator=(Connection &&other) noexcept;

    bool valid() const { return fd_ >= 0; }

    // Envia uma requisição no formato do protocolo host:
    // 4 dígitos hex de tamanho + payload ASCII.
    bool send_request(const std::string &payload);

    // Lê o status OKAY/FAIL. Em Fail, preenche err_message com a mensagem
    // retornada pelo servidor (se houver).
    Status read_status(std::string &err_message);

    // Lê até o peer fechar a conexão (usado por shell:, que é stream
    // bruto sem framing de tamanho).
    std::string read_until_eof();

    // Lê uma resposta com framing de tamanho (4 hex + N bytes), usada por
    // host:devices, host:devices-l etc.
    std::string read_framed();

    // ---- primitivas binárias cruas (usadas pelo protocolo sync:, que é
    // binário little-endian — diferente do framing hex-ASCII acima) ----

    // Escreve exatamente `len` bytes. Retorna false em falha/short write.
    bool write_raw(const void *data, size_t len);

    // Lê exatamente `len` bytes. Retorna false em EOF/erro antes de
    // completar (short read não é sucesso parcial, é falha).
    bool read_raw(void *buf, size_t len);

private:
    int fd_;
};

} // namespace adb
