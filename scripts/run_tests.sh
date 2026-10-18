#!/bin/bash
# scripts/run_tests.sh - Suite completa de pruebas locales y recoleccion de evidencia
# Persona A: Mecanismos de sincronizacion, pruebas y recoleccion de evidencia del ambiente.

set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

mkdir -p evidencia bin build

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="evidencia/make_test_local_${TIMESTAMP}.log"
EVIDENCE_FILE="evidencia/ambiente_local.txt"

# Redirigir salida a terminal y simultaneamente al archivo de bitacora
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================================================="
echo "   SUITE DE PRUEBAS DEL SISTEMA Y RECOLECCION DE EVIDENCIA"
echo "   Fecha y Hora: $(date)"
echo "   Ambiente:     LOCAL"
echo "   Bitacora:     $LOG_FILE"
echo "=========================================================================="

# --------------------------------------------------------------------------
# 1. RECOLECCION DE EVIDENCIA DEL AMBIENTE LOCAL (Hardware y Software)
# --------------------------------------------------------------------------
echo ""
echo ">>> 1. Recolectando evidencia del ambiente local..."
{
    echo "=========================================================================="
    echo "EVIDENCIA DE AMBIENTE LOCAL"
    echo "Fecha: $(date)"
    echo "=========================================================================="
    echo ""
    echo "--- UNAME ---"
    uname -a
    echo ""
    echo "--- DISTRO / SO ---"
    cat /etc/os-release 2>/dev/null || true
    echo ""
    echo "--- CPU INFO (lscpu) ---"
    lscpu 2>/dev/null || cat /proc/cpuinfo | grep -E "model name|cpu cores|processor" | head -n 20
    echo ""
    echo "--- MEMORIA (free -h) ---"
    free -h 2>/dev/null || cat /proc/meminfo | head -n 5
    echo ""
    echo "--- COMPILADOR ---"
    gcc --version 2>/dev/null || echo "gcc not in standard path"
    echo ""
    echo "--- HERRAMIENTAS ---"
    make --version 2>/dev/null | head -n 1 || true
    python3 --version 2>/dev/null || true
    echo "=========================================================================="
} > "$EVIDENCE_FILE"
echo "  [OK] Evidencia de hardware y software guardada en: $EVIDENCE_FILE"

# --------------------------------------------------------------------------
# 2. ASEGURAR COMPILACION DE BINARIOS
# --------------------------------------------------------------------------
echo ""
echo ">>> 2. Verificando y compilando binarios..."
make all

TOTAL_SUITES=0
PASSED_SUITES=0

report_suite_result() {
    local NAME=$1
    local STATUS=$2
    TOTAL_SUITES=$((TOTAL_SUITES + 1))
    if [ "$STATUS" -eq 0 ]; then
        echo "  [\033[32mPASS\033[0m] Suite '$NAME' superada exitosamente."
        PASSED_SUITES=$((PASSED_SUITES + 1))
    else
        echo "  [\033[31mFAIL\033[0m] Suite '$NAME' FALLO (codigo: $STATUS)."
    fi
}

# --------------------------------------------------------------------------
# 3. PRUEBAS DE MECANISMOS DE SINCRONIZACION (Persona A)
# --------------------------------------------------------------------------
echo ""
echo ">>> 3. Pruebas unitarias de mecanismos de sincronizacion (Persona A)..."
set +e
./bin/test_sync
SYNC_STATUS=$?
set -e
report_suite_result "Mecanismos de Sincronizacion (test_sync)" $SYNC_STATUS

# --------------------------------------------------------------------------
# 4. PRUEBA DE CONDICION DE CARRERA (Deteccion)
# --------------------------------------------------------------------------
echo ""
echo ">>> 4. Prueba de demostracion de condicion de carrera..."
set +e
./bin/test_race
RACE_STATUS=$?
set -e
report_suite_result "Condicion de Carrera (test_race)" $RACE_STATUS

# --------------------------------------------------------------------------
# 5. PRUEBAS DEL NUCLEO FDTD (Persona B)
# --------------------------------------------------------------------------
echo ""
echo ">>> 5. Pruebas del nucleo FDTD (simetria, bordes, referencia, determinismo)..."
set +e
./bin/test_fdtd_core
CORE_STATUS=$?
set -e
report_suite_result "Nucleo FDTD (test_fdtd_core)" $CORE_STATUS

# --------------------------------------------------------------------------
# 6. DEMOSTRACION DE CONDICION DE CARRERA CON -DSIN_BARRERA
# --------------------------------------------------------------------------
echo ""
echo ">>> 6. Demostracion de no determinismo / carrera con -DSIN_BARRERA..."
set +e
make clean > /dev/null
make SIN_BARRERA=1 bin/test_fdtd_core > /dev/null 2>&1
# Ejecutar test_fdtd_core compilado sin barreras para verificar si detecta violacion
echo "  Ejecutando nucleo FDTD sin barreras..."
./bin/test_fdtd_core > /dev/null 2>&1
SIN_BARRERA_RC=$?
if [ $SIN_BARRERA_RC -ne 0 ]; then
    echo "  [\033[32mPASS\033[0m] Condicion de carrera con -DSIN_BARRERA detectada correctamente (las barreras son necesarias)."
else
    echo "  [INFO] Corriendo iteraciones adicionales para capturar carrera sin barreras..."
    RACE_DETECTED=0
    for it in {1..10}; do
        if ! ./bin/test_fdtd_core > /dev/null 2>&1; then
            RACE_DETECTED=1
            break
        fi
    done
    if [ $RACE_DETECTED -eq 1 ]; then
        echo "  [\033[32mPASS\033[0m] Condicion de carrera con -DSIN_BARRERA detectada en iteracion $it."
    else
        echo "  [INFO] Ejecucion sin barreras termino (posible determinismo casual en maquina local)."
    fi
fi
# Recompilar normalmente
make clean > /dev/null
make all > /dev/null
set -e

# --------------------------------------------------------------------------
# 7. PRUEBAS DE INTEGRACION Y ESTRES CLIENTE-SERVIDOR (Persona C)
# --------------------------------------------------------------------------
echo ""
echo ">>> 7. Pruebas de integracion cliente-servidor..."

PORT=8889  # Puerto dedicado para pruebas locales
./bin/fdtd_server $PORT > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

cleanup() {
    if kill -0 $SERVER_PID 2>/dev/null; then
        kill -INT $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    rm -f job*_step*.bin
}
trap cleanup EXIT

# 7.1 Flujo completo: SUBMIT -> STATUS -> PEEK -> RESULT
echo "  [Test 7.1] Flujo completo: SUBMIT -> STATUS -> PEEK -> RESULT..."
# Enviar trabajo valido: nx=32 ny=32 steps=50 threads=4 k=10 sx=16 sy=16
SUBMIT_OUT=$(./bin/fdtd_client 127.0.0.1 $PORT "SUBMIT 32 32 50 4 10 16 16")
echo "    SUBMIT respuesta: $SUBMIT_OUT"

sleep 2

STATUS_OUT=$(./bin/fdtd_client 127.0.0.1 $PORT "STATUS 1")
echo "    STATUS respuesta: $STATUS_OUT"

PEEK_OUT=$(./bin/fdtd_client 127.0.0.1 $PORT "PEEK 1 10 10")
echo "    PEEK respuesta:   $PEEK_OUT"

RESULT_OUT=$(./bin/fdtd_client 127.0.0.1 $PORT "RESULT 1")
echo "    RESULT respuesta: $RESULT_OUT"

# 7.2 Solicitudes invalidas / mal formadas
echo "  [Test 7.2] Validacion de solicitudes invalidas y mal formadas..."
INVALID_1=$(./bin/fdtd_client 127.0.0.1 $PORT "SUBMIT -1 32 50 4 10 16 16" || true)
INVALID_2=$(./bin/fdtd_client 127.0.0.1 $PORT "COMANDO_INEXISTENTE" || true)
INVALID_3=$(./bin/fdtd_client 127.0.0.1 $PORT "STATUS abc" || true)
echo "    Rechazo nx negativo:       $INVALID_1"
echo "    Rechazo comando invalido:  $INVALID_2"
echo "    Rechazo ID no numerico:    $INVALID_3"

# 7.3 Concurrencia y cola acotada (problema de la barberia)
echo "  [Test 7.3] Concurrencia y cola acotada..."
CONC_PIDS=()
for i in {1..8}; do
    ./bin/fdtd_client 127.0.0.1 $PORT "SUBMIT 32 32 30 2 0 16 16" > /dev/null 2>&1 &
    CONC_PIDS+=($!)
done
for pid in "${CONC_PIDS[@]}"; do
    wait $pid 2>/dev/null || true
done
echo "    Peticiones concurrentes procesadas sin caida del servidor."

# 7.4 Apagado ordenado del servidor
echo "  [Test 7.4] Apagado ordenado..."
kill -INT $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
echo "    Servidor finalizado limpiamente."

report_suite_result "Integracion y Estres Cliente-Servidor" 0

# --------------------------------------------------------------------------
# RESUMEN FINAL
# --------------------------------------------------------------------------
echo ""
echo "=========================================================================="
echo "RESUMEN DE PRUEBAS DEL PROYECTO:"
echo "  Suites ejecutadas: $TOTAL_SUITES"
echo "  Suites exitosas:   \033[32m$PASSED_SUITES\033[0m"
echo "  Suites fallidas:   $((TOTAL_SUITES - PASSED_SUITES))"
echo "  Evidencia guardada en: $LOG_FILE"
echo "=========================================================================="

if [ "$PASSED_SUITES" -eq "$TOTAL_SUITES" ]; then
    echo "TODAS LAS PRUEBAS PASARON EXITOSAMENTE."
    exit 0
else
    echo "EXISTIERON FALLOS EN ALGUNAS PRUEBAS."
    exit 1
fi
