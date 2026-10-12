#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include "../includes/fdtd.h"
#include "../includes/solver.h"

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "Uso: %s nx ny pasos hilos k\n", argv[0]);
        return EXIT_FAILURE;
    }

    int nx = atoi(argv[1]);
    int ny = atoi(argv[2]);
    int steps = atoi(argv[3]);
    int threads = atoi(argv[4]);
    int k = atoi(argv[5]);

    if (nx < 3 || ny < 3 || steps <= 0 || threads < 1 || threads > nx - 2 || k < 0) {
        fprintf(stderr, "Error: Parámetros inválidos para simulación FDTD.\n");
        return EXIT_FAILURE;
    }

    int sx = nx / 2;
    int sy = ny / 2;

    double t0 = get_time_sec();
    fdtdGrid grid;
    if (fdtd_grid_init(&grid, nx, ny) != 0) {
        fprintf(stderr, "Error: no se pudo inicializar la malla.\n");
        return EXIT_FAILURE;
    }

    bqueue_t snapshot_q;
    int has_snapshots = (k > 0);
    pthread_t writer_tid;

    if (has_snapshots) {
        if (bqueue_init(&snapshot_q, 4) != 0) {
            fprintf(stderr, "Error: no se pudo inicializar la cola de snapshots.\n");
            fdtd_grid_free(&grid);
            return EXIT_FAILURE;
        }
        if (pthread_create(&writer_tid, NULL, snapshot_writer_thread, &snapshot_q) != 0) {
            fprintf(stderr, "Error: no se pudo crear el hilo escritor.\n");
            bqueue_destroy(&snapshot_q);
            fdtd_grid_free(&grid);
            return EXIT_FAILURE;
        }
    }

    double t1 = get_time_sec();
    double init_time = t1 - t0;

    double t_comp_start = get_time_sec();
    int status = fdtd_run_extended(&grid, steps, threads, sx, sy, k, 1, NULL, has_snapshots ? &snapshot_q : NULL);
    double t_comp_end = get_time_sec();
    double comp_time = t_comp_end - t_comp_start;

    double io_time = 0.0;
    if (has_snapshots) {
        double t_io_start = get_time_sec();
        bqueue_stop(&snapshot_q);
        pthread_join(writer_tid, NULL);
        bqueue_destroy(&snapshot_q);
        double t_io_end = get_time_sec();
        io_time = t_io_end - t_io_start;
    }

    double sum_ez = fdtd_sum_abs_ez(&grid);
    fdtd_grid_free(&grid);

    if (status != 0) {
        fprintf(stderr, "Error durante la ejecución del solver.\n");
        return EXIT_FAILURE;
    }

    /* CSV */
    printf("nx,ny,pasos,hilos,k,t_init,t_comp,t_io,t_total,sum_abs_ez\n");
    printf("%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.10e\n",
           nx, ny, steps, threads, k,
           init_time, comp_time, io_time, init_time + comp_time + io_time,
           sum_ez);

    return EXIT_SUCCESS;
}
