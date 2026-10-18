#!/bin/bash
# scripts/collect_bench.sh - Envoltorio para compatibilidad con bench.sh (CI-0117)
# Redirige la ejecucion a scripts/bench.sh para mediciones locales.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$DIR/bench.sh" "$@"
