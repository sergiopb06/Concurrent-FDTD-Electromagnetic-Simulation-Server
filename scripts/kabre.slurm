#!/bin/bash
#SBATCH --job-name=fdtd_bench
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=00:30:00
#SBATCH --output=bench_%j.out

# Cargar módulos si es necesario (ej: module load gcc)
# module load gcc

# Asegurar compilación
make clean
make

# Ejecutar recolección
./scripts/collect_bench.sh kabre 32
