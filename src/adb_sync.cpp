// src/adb_sync.cpp

#include "adb_sync.hpp"
#include "adb_connection.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace adb::sync
{

namespace
{
constexpr uint32_t kMaxChunk = 64 * 1024; // limite de payload do protocolo sync

// Escreve um pacote sync: id de 4 bytes ASCII + tamanho em 4 bytes
// little-endian + payload opcional. Binário — não confundir com o
// framing hex-ASCII usado por host:/shell:.
bool write_packet(Connection &conn, const char id[4], const void *data, uint32_t len)
{
    if (!conn.write_raw(id, 4))
        return false;

    uint8_t len_le[4] = {
        static_cast<uint8_t>(len & 0xff),
        static_cast<uint8_t>((len >> 8) & 0xff),
        static_cast<uint8_t>((len >> 16) & 0xff),
        static_cast<uint8_t>((len >> 24) & 0xff),
    };
    if (!conn.write_raw(len_le, 4))
        return false;

    if (len > 0 && data && !conn.write_raw(data, len))
        return false;

    return true;
}

// Lê o cabeçalho de um pacote sync (id de 4 bytes + tamanho LE).
bool read_header(Connection &conn, char id_out[4], uint32_t &len_out)
{
    if (!conn.read_raw(id_out, 4))
        return false;

    uint8_t len_le[4];
    if (!conn.read_raw(len_le, 4))
        return false;

    len_out = static_cast<uint32_t>(len_le[0]) | (static_cast<uint32_t>(len_le[1]) << 8) |
              (static_cast<uint32_t>(len_le[2]) << 16) | (static_cast<uint32_t>(len_le[3]) << 24);
    return true;
}

// Handshake comum: host:transport:<serial> (OKAY) + sync: (OKAY). Depois
// disso a conexão vira binária pura até fechar.
bool enter_sync_mode(Connection &conn, const std::string &serial, std::string &out_message)
{
    if (!conn.valid())
    {
        out_message = "nao foi possivel conectar ao adb server";
        return false;
    }

    std::string transport_req =
        serial.empty() ? std::string("host:transport-any") : "host:transport:" + serial;

    std::string err;
    if (!conn.send_request(transport_req) || conn.read_status(err) != Status::Okay)
    {
        out_message = "transport falhou: " + (err.empty() ? std::string("desconhecido") : err);
        return false;
    }

    if (!conn.send_request("sync:") || conn.read_status(err) != Status::Okay)
    {
        out_message = "sync: falhou: " + (err.empty() ? std::string("desconhecido") : err);
        return false;
    }

    return true;
}
} // namespace

bool push(const std::string &serial, const std::string &local_path,
          const std::string &remote_path, std::string &out_message)
{
    FILE *f = std::fopen(local_path.c_str(), "rb");
    if (!f)
    {
        out_message = "nao foi possivel abrir arquivo local: " + local_path;
        return false;
    }

    // Preserva permissões e mtime do arquivo local, como o adb push real.
    struct stat st{};
    int mode = 0644;
    long mtime = 0;
    if (stat(local_path.c_str(), &st) == 0)
    {
        mode = st.st_mode & 0777;
        mtime = static_cast<long>(st.st_mtime);
    }

    Connection conn;
    if (!enter_sync_mode(conn, serial, out_message))
    {
        std::fclose(f);
        return false;
    }

    // Cabeçalho SEND: "<remote_path>,<mode_octal_decimal>"
    char header_buf[4096];
    int header_len =
        std::snprintf(header_buf, sizeof(header_buf), "%s,%d", remote_path.c_str(), mode);
    if (header_len < 0 || static_cast<size_t>(header_len) >= sizeof(header_buf))
    {
        out_message = "caminho remoto muito longo";
        std::fclose(f);
        return false;
    }

    if (!write_packet(conn, "SEND", header_buf, static_cast<uint32_t>(header_len)))
    {
        out_message = "falha ao enviar cabecalho SEND";
        std::fclose(f);
        return false;
    }

    std::vector<char> chunk(kMaxChunk);
    size_t n;
    while ((n = std::fread(chunk.data(), 1, chunk.size(), f)) > 0)
    {
        if (!write_packet(conn, "DATA", chunk.data(), static_cast<uint32_t>(n)))
        {
            out_message = "falha ao enviar DATA";
            std::fclose(f);
            return false;
        }
    }
    if (std::ferror(f))
    {
        out_message = "erro lendo arquivo local: " + local_path;
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    // DONE: o campo "tamanho" aqui carrega o mtime (sem payload).
    if (!write_packet(conn, "DONE", nullptr, static_cast<uint32_t>(mtime)))
    {
        out_message = "falha ao enviar DONE";
        return false;
    }

    char resp_id[4];
    uint32_t resp_len = 0;
    if (!read_header(conn, resp_id, resp_len))
    {
        out_message = "sem resposta do device apos DONE";
        return false;
    }

    if (std::memcmp(resp_id, "OKAY", 4) == 0)
    {
        out_message = remote_path;
        return true;
    }

    if (std::memcmp(resp_id, "FAIL", 4) == 0)
    {
        std::string msg(resp_len, '\0');
        conn.read_raw(msg.data(), resp_len);
        out_message = "FAIL: " + msg;
        return false;
    }

    out_message = "resposta inesperada do device";
    return false;
}

bool pull(const std::string &serial, const std::string &remote_path,
          const std::string &local_path, std::string &out_message)
{
    Connection conn;
    if (!enter_sync_mode(conn, serial, out_message))
        return false;

    if (!write_packet(conn, "RECV", remote_path.c_str(), static_cast<uint32_t>(remote_path.size())))
    {
        out_message = "falha ao enviar RECV";
        return false;
    }

    FILE *f = std::fopen(local_path.c_str(), "wb");
    if (!f)
    {
        out_message = "nao foi possivel criar arquivo local: " + local_path;
        return false;
    }

    while (true)
    {
        char id[4];
        uint32_t len = 0;
        if (!read_header(conn, id, len))
        {
            out_message = "conexao perdida durante pull";
            std::fclose(f);
            return false;
        }

        if (std::memcmp(id, "DATA", 4) == 0)
        {
            std::vector<char> chunk(len);
            if (len > 0 && !conn.read_raw(chunk.data(), len))
            {
                out_message = "falha ao ler DATA";
                std::fclose(f);
                return false;
            }
            std::fwrite(chunk.data(), 1, chunk.size(), f);
        }
        else if (std::memcmp(id, "DONE", 4) == 0)
        {
            std::fclose(f);
            out_message = local_path;
            return true;
        }
        else if (std::memcmp(id, "FAIL", 4) == 0)
        {
            std::string msg(len, '\0');
            conn.read_raw(msg.data(), len);
            out_message = "FAIL: " + msg;
            std::fclose(f);
            return false;
        }
        else
        {
            out_message = "pacote inesperado durante pull";
            std::fclose(f);
            return false;
        }
    }
}

} // namespace adb::sync
