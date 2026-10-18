#!/bin/bash
# scripts/bench.sh - Script de mediciones de rendimiento local para FDTD
#
# Ejecuta los escenarios de medicion localmente con multiples hilos y 5 repeticiones,
# generando archivos CSV limpios y ejecutando scripts/analyze.py para graficar.

set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

mkdir -p evidencia bin

# Compilar fdtd_bench si no existe
if [ ! -f bin/fdtd_bench ]; then
    echo "Compilando bin/fdtd_bench..."
    make bench
fi

PYTHON_CMD="python3"
if [ -f "./venv/bin/python3" ]; then
    PYTHON_CMD="./venv/bin/python3"
fi

# Detectar hilos locales por defecto (1, 2, 4, 8, 16 para maquina de 4 nucleos / 8 hilos)
PHYSICAL_CORES=$(grep -m1 'cpu cores' /proc/cpuinfo 2>/dev/null | awk '{print $4}' || echo 4)
TOTAL_THREADS=$(nproc 2>/dev/null || echo 8)
OVERSUB=$((TOTAL_THREADS * 2))

THREADS_DEFAULT="1 2 4 8 16"

# Modo rapido (--quick) o completo (default)
MODE="full"
if [ "$1" == "--quick" ] || [ "$1" == "-q" ]; then
    MODE="quick"
    shift
fi

THREADS_LIST="${*:-$THREADS_DEFAULT}"

echo "==============================================================="
echo "   BENCHMARK LOCAL DE RENDIMIENTO (AMDAHL)"
echo "   Ambiente: LOCAL ($(uname -s) $(uname -m), ${PHYSICAL_CORES} nucleos, ${TOTAL_THREADS} hilos logicos)"
echo "   Hilos a medir: $THREADS_LIST"
echo "   Modo: $MODE (5 repeticiones por configuracion)"
echo "==============================================================="

run_scenario() {
    local SCENARIO_ID=$1
    local NX=$2
    local NY=$3
    local STEPS=$4
    local K=$5
    local DESC=$6
    local CSV_OUT="evidencia/bench_local_${SCENARIO_ID}.csv"

    echo ""
    echo ">>> Iniciando Escenario ${SCENARIO_ID}: ${DESC}"
    echo "    Parametros: nx=${NX}, ny=${NY}, pasos=${STEPS}, k=${K}"
    echo "    Salida CSV: ${CSV_OUT}"

    # Escribir encabezado CSV
    echo "nx,ny,pasos,threads,k,t_init,t_comp,t_io,t_total,sum_abs_ez" > "$CSV_OUT"

    for TH in $THREADS_LIST; do
        echo -n "    Midiendo con ${TH} hilo(s)... "
        for REP in {1..5}; do
            # fdtd_bench retorna 2 lineas: encabezado y valores. Extraer la segunda linea con tail -n 1
            OUTPUT_LINE=$(./bin/fdtd_bench "$NX" "$NY" "$STEPS" "$TH" "$K" | tail -n 1)
            echo "$OUTPUT_LINE" >> "$CSV_OUT"
            echo -n "."
        done
        echo " OK"
    done

    # Analizar y generar graficos para este escenario
    echo "    Generando analisis y graficos con analyze.py..."
    $PYTHON_CMD scripts/analyze.py "$CSV_OUT" --output_dir evidencia
}

if [ "$MODE" == "quick" ]; then
    # Escenarios escalados para verificacion rapida local
    run_scenario "E1" 512 512 100 0 "Malla 512x512, 100 pasos, sin instantaneas (k=0)"
    run_scenario "E2" 512 512 100 10 "Malla 512x512, 100 pasos, con instantaneas (k=10)"
    run_scenario "E3" 128 128 100 0 "Malla 128x128, 100 pasos, sin instantaneas (k=0)"
else
    # Escenarios formales del enunciado (Tabla 4)
    run_scenario "E1" 2048 2048 1000 0 "Malla 2048x2048, 1000 pasos, sin instantaneas (k=0)"
    run_scenario "E2" 2048 2048 1000 10 "Malla 2048x2048, 1000 pasos, con instantaneas cada 10 pasos (k=10)"
    run_scenario "E3" 256 256 1000 0 "Malla 256x256, 1000 pasos, sin instantaneas (k=0)"
fi

echo ""
echo "==============================================================="
echo "   BENCHMARKS LOCALES COMPLETADOS"
echo "   Resultados y graficos disponibles en: evidencia/"
echo "==============================================================="
