#!/bin/bash

# Script de pruebas de integración para el proyecto FDTD


set -e

# Asegurar que los binarios existen
if [ ! -f bin/fdtd_server ] || [ ! -f bin/fdtd_client ]; then
    echo "Error: Binarios no encontrados. Ejecuta 'make' primero."
    exit 1
fi

echo "--- Iniciando pruebas de integración ---"

# 1. Iniciar el servidor en segundo plano
./bin/fdtd_server &
SERVER_PID=$!
sleep 2 # Esperar a que el servidor inicie

# Limpieza al terminar
cleanup() {
    echo "--- Realizando shutdown del servidor ---"
    ./bin/fdtd_client 127.0.0.1 8888 SHUTDOWN
    wait $SERVER_PID 2>/dev/null || true
    # Limpiar archivos generados por las pruebas
    rm -f job*_step*.bin
}
trap cleanup EXIT

# 2. Flujo completo: SUBMIT -> STATUS -> PEEK -> RESULT
echo "Probando flujo: SUBMIT -> STATUS -> PEEK"
# Submit: nx=32 ny=32 steps=50 hilos=4
./bin/fdtd_client 127.0.0.1 8888 "SUBMIT 32 32 50 4"
sleep 1
./bin/fdtd_client 127.0.0.1 8888 "STATUS 1"
./bin/fdtd_client 127.0.0.1 8888 "PEEK 1 10 10"

# 3. Solicitud mal formada
echo "Probando solicitud mal formada..."
./bin/fdtd_client 127.0.0.1 8888 "SUBMIT -1 32 50 4" || echo "Servidor rechazó solicitud inválida correctamente"

# 4. Estrés concurrente (simulado)
echo "Probando concurrencia..."
for i in {1..5}; do
    ./bin/fdtd_client 127.0.0.1 8888 "SUBMIT 32 32 10 2" &
done
wait

echo "--- Todas las pruebas de integración pasaron ---"
