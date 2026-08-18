#!/bin/bash

# Versão simplificada
ULTIMO=$(ls -1 [0-9][0-9]-exemplo.lua 2>/dev/null | sort -r | head -1)

if [ -z "$ULTIMO" ]; then
    echo "Nenhum arquivo encontrado. Criando 01-exemplo.lua..."
    echo "-- 01-exemplo.lua" > 01-exemplo.lua
else
    NUM=$(echo "$ULTIMO" | cut -c1-2)
    PROXIMO=$((10#$NUM + 1))
    NOVO=$(printf "%02d" $PROXIMO)
    cp "$ULTIMO" "${NOVO}-exemplo.lua"
    echo "Criado: ${NOVO}-exemplo.lua (copiado de $ULTIMO)"
fi

ls -1 [0-9][0-9]-exemplo.lua
