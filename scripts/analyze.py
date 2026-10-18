import pandas as pd
import matplotlib.pyplot as plt
import sys

def analyze(env_name, csv_file):
    df = pd.read_csv(csv_file)
    summary = df.groupby('threads')['total'].agg(['median', 'std'])
    
    # 1. Speedup
    t1 = summary.loc[1, 'median']
    summary['speedup'] = t1 / summary['median']
    
    # 2. Plotting
    plt.figure(figsize=(10, 6))
    plt.errorbar(summary.index, summary['median'], yerr=summary['std'], label='Tiempo Real', fmt='-o')
    plt.title(f'Rendimiento en {env_name}')
    plt.xlabel('Threads')
    plt.ylabel('Tiempo (segundos)')
    plt.grid(True)
    plt.savefig(f'plot_time_{env_name}.png')
    
    plt.figure(figsize=(10, 6))
    plt.plot(summary.index, summary['speedup'], marker='s', label='Speedup observado')
    plt.plot(summary.index, summary.index, linestyle='--', color='gray', label='Amdahl Ideal')
    plt.title(f'Aceleración (Speedup) en {env_name}')
    plt.xlabel('Threads')
    plt.ylabel('Speedup')
    plt.legend()
    plt.savefig(f'plot_speedup_{env_name}.png')
    
    print(f"Análisis para {env_name} generado.")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Uso: python analyze.py <nombre> <archivo.csv>")
    else:
        analyze(sys.argv[1], sys.argv[2])
