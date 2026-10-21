Concurrent-FDTD-Electromagnetic-Simulation-Server
Creador por: 

    -Sergio Peralta Barboza
    -Carlos Manuel Guido Gómez
    -Jocksuar Thompson Matarrita

Servidor concurrente de simulaciones electromagnéticas.

Requisitos

Linux

Compilar

bash
make clean && make


Ejecutar

Terminal 1, el servidor.

bash
mkdir -p /tmp/fdtd_run && cd /tmp/fdtd_run
/ruta/al/repo/bin/fdtd_server 8080


Terminal 2, el cliente. El host debe conectar a 120.0.0.1:

bash
C=/ruta/al/repo/bin/fdtd_client

#--------n de filas y columnas, pasos, hilos, k pasos para guardar, ubicacion de inicio de simulacion  
$C 127.0.0.1 8080 SUBMIT 1024 1024 3000 4 100 512 512   # OK <id>, BUSY o ERROR
$C 127.0.0.1 8080 STATUS 1                              # RUNNING 
$C 127.0.0.1 8080 PEEK 1 512 512                        # VALUE <paso> <valor>
$C 127.0.0.1 8080 RESULT 1                              # resultado final
$C 127.0.0.1 8080 SHUTDOWN                              # BYE
```


Benchmark

bash
./bin/fdtd_bench nx ny pasos hilos k
./bin/fdtd_bench 2048 2048 1000 4 0


Imprime una línea CSV con `t_init`, `t_comp`, `t_io`, `t_total` y `sum_abs_ez`.

Pruebas

bash
F="-std=c17 -Wall -Wextra -pedantic -O2 -pthread -Iincludes -I."
SRC="src/fdtd.c src/solver.c src/barrier.c src/rwlock.c src/bqueue.c src/snapshot.c src/state.c"

Primitivas de sincronización
gcc $F tests/test_sync.c src/barrier.c src/rwlock.c src/bqueue.c src/snapshot.c src/state.c -o /tmp/t_sync -lm && /tmp/t_sync

Núcleo FDTD 
gcc $F tests/test_fdtd_core.c $SRC -o /tmp/t_core -lm && /tmp/t_core


Demostrar la condición de carrera

Compilar el núcleo sin las barreras debe hacer fallar la prueba de determinismo:

bash
gcc $F -DSIN_BARRERA tests/test_fdtd_core.c $SRC -o /tmp/t_core_race -lm && /tmp/t_core_race | grep -i fail
