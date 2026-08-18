CC       := gcc
CFLAGS   := -std=c11 -O2 -shared -fPIC -Wall -Wextra

SRC      := src/adb_bridge.c
OUT_DIR  := .
OUT      := $(OUT_DIR)/adb_bridge.so

.PHONY: all install clean check-deps

all: install

check-deps:
	@command -v gcc >/dev/null || (echo "ERRO: gcc não encontrado" && exit 1)

install: check-deps
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)
	@echo "✓ Instalado em $(OUT)"

clean:
	@rm -f $(OUT)
	@echo "✓ $(OUT) removido"
