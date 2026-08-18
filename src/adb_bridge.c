// src/adb_bridge.c
//
// Bridge nativo para o "adb server" local (127.0.0.1:5037), usando o
// protocolo host do ADB diretamente via socket TCP. Evita fork+exec do
// binário `adb` a cada chamada.
//
// Requer: `adb start-server` já rodando (o daemon padrão do Android SDK).

#define _POSIX_C_SOURCE 200809L /* necessário para strdup em -std=c11 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define ADB_HOST "127.0.0.1"
#define ADB_PORT 5037
#define CHUNK    4096

static int adb_connect_socket(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(ADB_PORT);
    inet_pton(AF_INET, ADB_HOST, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Envia uma requisição no formato do protocolo host: 4 dígitos hex de
// tamanho + payload ASCII. Ex: "000Chost:version"
static int send_request(int fd, const char *payload)
{
    char header[5];
    size_t len = strlen(payload);
    /* protocolo host usa exatamente 4 dígitos hex; comandos maiores que
     * 0xffff bytes não são suportados por este bridge. */
    snprintf(header, sizeof(header), "%04x", (unsigned int)(len & 0xffffu));

    if (write(fd, header, 4) != 4)
        return -1;
    if (write(fd, payload, len) != (ssize_t)len)
        return -1;
    return 0;
}

// Lê o status OKAY/FAIL. Em FAIL, preenche `err` com a mensagem.
// Retorna 1 = OKAY, 0 = FAIL, -1 = erro de I/O.
static int read_status(int fd, char *err, size_t errcap)
{
    char status[4];
    if (read(fd, status, 4) != 4)
        return -1;

    if (memcmp(status, "OKAY", 4) == 0)
        return 1;

    if (memcmp(status, "FAIL", 4) == 0) {
        char lenhex[5] = {0};
        if (read(fd, lenhex, 4) == 4) {
            long msglen = strtol(lenhex, NULL, 16);
            if (msglen > 0 && err && errcap > 0) {
                size_t toread = (size_t)msglen < errcap - 1 ? (size_t)msglen : errcap - 1;
                ssize_t n = read(fd, err, toread);
                if (n > 0)
                    err[n] = '\0';
            }
        }
        return 0;
    }
    return -1;
}

// Lê até o peer fechar a conexão (usado em `shell:`, que é um stream
// bruto sem framing de tamanho).
static char *read_until_eof(int fd)
{
    size_t cap = CHUNK;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf)
        return NULL;

    ssize_t n;
    char tmp[CHUNK];
    while ((n = read(fd, tmp, sizeof(tmp))) > 0) {
        if (len + (size_t)n + 1 > cap) {
            cap = (len + (size_t)n + 1) * 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        memcpy(buf + len, tmp, (size_t)n);
        len += (size_t)n;
    }
    buf[len] = '\0';
    return buf;
}

// ---- API pública (exportada para LuaJIT FFI) ----

// adb_shell: equivalente a `adb -s <serial> shell <cmd>`.
// serial pode ser NULL/vazio para usar "host:transport-any" (device único).
// Retorna string malloc'd (chamador deve liberar com adb_free).
char *adb_shell(const char *serial, const char *cmd)
{
    int fd = adb_connect_socket();
    if (fd < 0)
        return strdup("ERROR: nao foi possivel conectar ao adb server (rode: adb start-server)");

    char err[256] = {0};
    char transport_req[300];

    if (serial && serial[0] != '\0')
        snprintf(transport_req, sizeof(transport_req), "host:transport:%s", serial);
    else
        snprintf(transport_req, sizeof(transport_req), "host:transport-any");

    if (send_request(fd, transport_req) < 0 || read_status(fd, err, sizeof(err)) != 1) {
        close(fd);
        char *out = malloc(300);
        snprintf(out, 300, "ERROR: transport falhou: %s", err[0] ? err : "desconhecido");
        return out;
    }

    char shell_req[CHUNK];
    snprintf(shell_req, sizeof(shell_req), "shell:%s", cmd);

    if (send_request(fd, shell_req) < 0 || read_status(fd, err, sizeof(err)) != 1) {
        close(fd);
        char *out = malloc(300);
        snprintf(out, 300, "ERROR: shell exec falhou: %s", err[0] ? err : "desconhecido");
        return out;
    }

    char *result = read_until_eof(fd);
    close(fd);
    return result ? result : strdup("");
}

// adb_devices: equivalente a `adb devices` (host:devices).
char *adb_devices(void)
{
    int fd = adb_connect_socket();
    if (fd < 0)
        return strdup("ERROR: nao foi possivel conectar ao adb server (rode: adb start-server)");

    char err[256] = {0};
    if (send_request(fd, "host:devices") < 0 || read_status(fd, err, sizeof(err)) != 1) {
        close(fd);
        char *out = malloc(300);
        snprintf(out, 300, "ERROR: %s", err[0] ? err : "desconhecido");
        return out;
    }

    char lenhex[5] = {0};
    if (read(fd, lenhex, 4) != 4) {
        close(fd);
        return strdup("ERROR: header de tamanho invalido");
    }
    long msglen = strtol(lenhex, NULL, 16);
    if (msglen < 0) {
        close(fd);
        return strdup("ERROR: tamanho invalido retornado pelo servidor");
    }

    char *buf = malloc((size_t)msglen + 1);
    if (!buf) { close(fd); return NULL; }

    size_t total = 0;
    while (total < (size_t)msglen) {
        ssize_t n = read(fd, buf + total, (size_t)msglen - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    buf[total] = '\0';
    close(fd);
    return buf;
}

// adb_free: libera strings retornadas por adb_shell/adb_devices.
void adb_free(char *ptr)
{
    free(ptr);
}
