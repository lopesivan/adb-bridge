CXX      := g++
CXXFLAGS := -std=c++17 -O2 -shared -fPIC -Wall -Wextra -Isrc

SRCS     := src/adb_connection.cpp \
            src/adb_host.cpp \
            src/adb_transport.cpp \
            src/adb_bridge_api.cpp

OUT_DIR  := .
OUT      := $(OUT_DIR)/adb_bridge.so

.PHONY: all install clean check-deps

all: install

check-deps:
	@command -v g++ >/dev/null || (echo "ERRO: g++ não encontrado" && exit 1)

install: check-deps
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRCS)
	@echo "✓ Instalado em $(OUT)"

clean:
	@rm -f $(OUT)
	@echo "✓ $(OUT) removido"
