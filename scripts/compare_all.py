import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

def analyze_all(csv_files):
    """
    csv_files: diccionario {'nombre_cluster': 'ruta_archivo.csv'}
    """
    plt.figure(figsize=(12, 7))
    
    # Colores para distinguir clusters
    colors = ['r', 'b', 'g']
    
    for i, (name, path) in enumerate(csv_files.items()):
        if not os.path.exists(path):
            print(f"Archivo no encontrado: {path}")
            continue
            
        df = pd.read_csv(path)
        summary = df.groupby('threads')['total'].agg(['median'])
        
        # Speedup vs Amdahl
        t1 = summary.loc[1, 'median']
        speedup = t1 / summary['median']
        
        plt.plot(summary.index, speedup, marker='o', linestyle='-', label=f'Speedup {name}', color=colors[i % len(colors)])
    
    # Línea ideal
    max_threads = 32 # Ajustar según tus datos
    plt.plot(range(1, max_threads+1), range(1, max_threads+1), linestyle='--', color='gray', label='Amdahl Ideal')
    
    plt.title('Comparativa de Aceleración entre Clusters')
    plt.xlabel('Número de Hilos')
    plt.ylabel('Speedup')
    plt.legend()
    plt.grid(True)
    plt.savefig('comparativa_clusters.png')
    print("Gráfica comparativa guardada como 'comparativa_clusters.png'")

if __name__ == "__main__":
    # Ajusta esto según tus archivos de resultados
    files = {
        'UCR': 'results_ucr.csv',
        'Kabre': 'results_kabre.csv',
        'Local': 'results_local.csv'
    }
    analyze_all(files)
