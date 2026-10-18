#!/usr/bin/env python3
"""
scripts/analyze.py - Analisis local de rendimiento y Ley de Amdahl

Procesa mediciones locales de fdtd_bench, calculando:
  - Mediana y dispersion de tiempo de ejecucion (5 repeticiones).
  - Estimacion de fraccion paralelizable 'p' a partir de los tiempos de 1 hilo.
  - Curva teorica de la Ley de Amdahl: S_amdahl(n) = 1 / ((1 - p) + p / n).
  - Aceleracion real observada: S(n) = T(1) / T(n).
  - Eficiencia: E(n) = S(n) / n.
  - Fraccion serial experimental de Karp-Flatt: e(n) = (1/S(n) - 1/n) / (1 - 1/n).

Genera graficos en PNG y un reporte tabular para el informe en LaTeX.
"""

import sys
import os
import argparse
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def parse_args():
    parser = argparse.ArgumentParser(description="Analisis de rendimiento FDTD local (Amdahl y Karp-Flatt)")
    parser.add_argument("csv_files", nargs="+", help="Ruta(s) al archivo CSV de resultados locales")
    parser.add_argument("--output_dir", default="evidencia", help="Directorio para guardar graficos (default: evidencia)")
    return parser.parse_args()

def analyze_dataset(csv_path, output_dir):
    if not os.path.exists(csv_path):
        print(f"Error: Archivo no encontrado: {csv_path}", file=sys.stderr)
        return

    os.makedirs(output_dir, exist_ok=True)
    base_name = os.path.splitext(os.path.basename(csv_path))[0]

    df = pd.read_csv(csv_path)

    # Normalizar nombres de columnas
    col_map = {
        'hilos': 'threads',
        't_total': 'total',
        't_comp': 'comp',
        't_init': 'init',
        't_io': 'io',
        'compute': 'comp'
    }
    df = df.rename(columns=col_map)

    if 'threads' not in df.columns or 'total' not in df.columns:
        print(f"Error: {csv_path} no contiene columnas esperadas (threads, total).", file=sys.stderr)
        return

    # Agrupar por cantidad de hilos: calcular mediana y desviacion estandar
    grouped = df.groupby('threads')
    summary = grouped['total'].agg(median='median', std='std').reset_index()

    # Si se reportaron etapas (comp, init, io), obtener la mediana de comp para estimar p
    has_stages = 'comp' in df.columns
    if has_stages:
        comp_summary = grouped['comp'].median().to_dict()
    else:
        comp_summary = {}

    # Linea base serial: T(1)
    t1_row = summary[summary['threads'] == 1]
    if t1_row.empty:
        # Si no hay medicion con 1 hilo, usar el menor numero de hilos como referencia
        min_th = summary['threads'].min()
        t1 = summary.loc[summary['threads'] == min_th, 'median'].values[0]
        print(f"Aviso: No se encontro medicion con 1 hilo. Usando {min_th} hilos como referencia.")
    else:
        t1 = t1_row['median'].values[0]

    # Estimar fraccion paralelizable p con 1 hilo: p = comp(1) / total(1)
    if has_stages and 1 in comp_summary and t1 > 0:
        p_est = comp_summary[1] / t1
        # Asegurar p dentro de limites validos [0, 1]
        p_est = min(max(p_est, 0.0), 1.0)
    else:
        p_est = 0.95  # Estimacion predeterminada si no hay desglose por etapas

    # Metricas
    threads = summary['threads'].values
    t_median = summary['median'].values
    t_std = summary['std'].fillna(0.0).values

    speedup_real = t1 / t_median
    speedup_amdahl = 1.0 / ((1.0 - p_est) + (p_est / threads))
    efficiency = speedup_real / threads

    # Karp-Flatt: e(n) = (1/S(n) - 1/n) / (1 - 1/n) para n > 1
    karp_flatt = []
    for n, s in zip(threads, speedup_real):
        if n > 1:
            e = ((1.0 / s) - (1.0 / n)) / (1.0 - (1.0 / n))
            karp_flatt.append(e)
        else:
            karp_flatt.append(np.nan)
    karp_flatt = np.array(karp_flatt)

    summary['speedup_real'] = speedup_real
    summary['speedup_amdahl'] = speedup_amdahl
    summary['efficiency'] = efficiency
    summary['karp_flatt'] = karp_flatt

    # Imprimir resumen tabular en consola
    print(f"\n==========================================================================")
    print(f"  ANALISIS DE RENDIMIENTO LOCAL: {base_name}")
    print(f"  Fraccion paralelizable estimada (Amdahl): p = {p_est:.4f} ({p_est*100:.2f}%)")
    print(f"==========================================================================")
    print(f"{'Hilos':>6} | {'Tiempo (s)':>16} | {'Speedup Real':>13} | {'Amdahl Pred':>12} | {'Eficiencia':>11} | {'Karp-Flatt e(n)':>16}")
    print(f"-" * 84)
    for _, row in summary.iterrows():
        th = int(row['threads'])
        t_str = f"{row['median']:.4f} ± {row['std']:.4f}"
        s_real = f"{row['speedup_real']:.2f}x"
        s_amd = f"{row['speedup_amdahl']:.2f}x"
        eff = f"{row['efficiency']*100:.1f}%"
        kf = f"{row['karp_flatt']:.4f}" if not np.isnan(row['karp_flatt']) else "N/A"
        print(f"{th:>6} | {t_str:>16} | {s_real:>13} | {s_amd:>12} | {eff:>11} | {kf:>16}")
    print(f"==========================================================================\n")

    # Guardar resumen en CSV
    summary_csv = os.path.join(output_dir, f"{base_name}_metricas.csv")
    summary.to_csv(summary_csv, index=False)
    print(f"Metricas guardadas en: {summary_csv}")

    # ==========================
    # GRAFICO 1: Speedup (Real vs Amdahl vs Lineal)
    # ==========================
    plt.figure(figsize=(9, 6))
    plt.plot(threads, speedup_real, marker='o', linewidth=2, color='#1f77b4', label='Aceleracion Real S(n)')
    plt.plot(threads, speedup_amdahl, marker='s', linestyle='--', linewidth=2, color='#ff7f0e',
             label=f'Prediccion Amdahl (p={p_est:.3f})')
    plt.plot(threads, threads, linestyle=':', color='#7f7f7f', label='Ideal Lineal (S(n) = n)')
    plt.title(f'Aceleracion (Speedup) Local - {base_name}', fontsize=13, fontweight='bold')
    plt.xlabel('Numero de Hilos (n)', fontsize=11)
    plt.ylabel('Speedup S(n)', fontsize=11)
    plt.xticks(threads)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(fontsize=10)
    out_speedup = os.path.join(output_dir, f"{base_name}_speedup.png")
    plt.savefig(out_speedup, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"Grafico guardado: {out_speedup}")

    # ==========================
    # GRAFICO 2: Eficiencia E(n)
    # ==========================
    plt.figure(figsize=(9, 6))
    plt.plot(threads, efficiency * 100, marker='^', linewidth=2, color='#2ca02c', label='Eficiencia Real E(n)')
    plt.axhline(100, linestyle=':', color='#7f7f7f', label='Eficiencia Ideal 100%')
    plt.title(f'Eficiencia Paralela Local - {base_name}', fontsize=13, fontweight='bold')
    plt.xlabel('Numero de Hilos (n)', fontsize=11)
    plt.ylabel('Eficiencia (%)', fontsize=11)
    plt.xticks(threads)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(fontsize=10)
    out_eff = os.path.join(output_dir, f"{base_name}_eficiencia.png")
    plt.savefig(out_eff, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"Grafico guardado: {out_eff}")

    # ==========================
    # GRAFICO 3: Metrica Karp-Flatt e(n)
    # ==========================
    valid_kf = summary[summary['threads'] > 1]
    if not valid_kf.empty:
        plt.figure(figsize=(9, 6))
        plt.plot(valid_kf['threads'], valid_kf['karp_flatt'], marker='d', linewidth=2, color='#d62728',
                 label='Fraccion serial e(n)')
        plt.title(f'Sobrecosto Experimental de Karp-Flatt - {base_name}', fontsize=13, fontweight='bold')
        plt.xlabel('Numero de Hilos (n)', fontsize=11)
        plt.ylabel('Fraccion Serial Experimental e(n)', fontsize=11)
        plt.xticks(valid_kf['threads'])
        plt.grid(True, linestyle='--', alpha=0.6)
        plt.legend(fontsize=10)
        out_kf = os.path.join(output_dir, f"{base_name}_karp_flatt.png")
        plt.savefig(out_kf, dpi=200, bbox_inches='tight')
        plt.close()
        print(f"Grafico guardado: {out_kf}")

    # ==========================
    # GRAFICO 4: Tiempo de Ejecucion con Barras de Error
    # ==========================
    plt.figure(figsize=(9, 6))
    plt.errorbar(threads, t_median, yerr=t_std, fmt='-o', color='#9467bd', ecolor='#8c564b',
                 elinewidth=2, capsize=4, label='Tiempo mediano ± std')
    plt.title(f'Tiempo de Ejecucion Local - {base_name}', fontsize=13, fontweight='bold')
    plt.xlabel('Numero de Hilos (n)', fontsize=11)
    plt.ylabel('Tiempo Total (s)', fontsize=11)
    plt.xticks(threads)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(fontsize=10)
    out_time = os.path.join(output_dir, f"{base_name}_tiempo.png")
    plt.savefig(out_time, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"Grafico guardado: {out_time}")

def main():
    args = parse_args()
    for csv_file in args.csv_files:
        analyze_dataset(csv_file, args.output_dir)

if __name__ == "__main__":
    main()
