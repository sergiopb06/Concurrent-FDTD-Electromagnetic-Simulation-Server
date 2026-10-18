#!/bin/bash
# Uso: ./scripts/collect_bench.sh <ambiente_nombre> <max_threads>

if [ "$#" -ne 2 ]; then
    echo "Uso: $0 <ambiente_nombre> <max_threads>"
    exit 1
fi

ENV=$1
MAX_THREADS=$2
OUTPUT="results_${ENV}.csv"

echo "threads,init,compute,barrier,io,total" > $OUTPUT

# Medir para 1, 2, 4, 8, 16... hasta MAX_THREADS
THREADS=1
while [ $THREADS -le $MAX_THREADS ]; do
    echo "Corriendo medición para $THREADS hilos en $ENV..."
    for i in {1..5}; do
        # Ejecuta benchmark: nx=512, ny=512, pasos=200, hilos=$THREADS, k=20
        RESULT=$(./bin/fdtd_bench 512 512 200 $THREADS 20)
        echo "$THREADS,$RESULT" >> $OUTPUT
    done
    
    # Lógica simple para incrementar hilos (1, 2, 4, 8, 16, 32...)
    if [ $THREADS -eq 1 ]; then THREADS=2; else THREADS=$((THREADS * 2)); fi
done

echo "Resultados guardados en $OUTPUT"
