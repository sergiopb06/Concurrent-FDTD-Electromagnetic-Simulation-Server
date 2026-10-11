#include <pthread.h>
#include "../includes/fdtd.h"
#include "../includes/solver.h"
#include "../includes/barrier.h"
#include "../includes/snapshot.h"

typedef struct {
    int thread_id;
    int row_start;
    int row_end;
    fdtdGrid *grid;
    int total_steps;
    int source_row;
    int source_column;
    barrier_t *barrier;
    int k;
    unsigned long job_id;
    published_state_t *state;
    bqueue_t *snapshot_queue;
} solver_thread_args_t;

static void *solver_thread(void *arg) {
    solver_thread_args_t *args = (solver_thread_args_t *)arg;

    for (int step = 0; step < args->total_steps; step++) {
        /* Phase (a): update magnetic field H */
        fdtd_update_h(args->grid, args->row_start, args->row_end);

#ifndef SIN_BARRERA
        barrier_wait(args->barrier);
#endif

        /* Phase (b): update electric field Ez */
        fdtd_update_e(args->grid, args->row_start, args->row_end);

        /* Phase (c): soft source (Ricker wavelet) */
        if (args->source_row >= args->row_start && args->source_row < args->row_end) {
            fdtd_add_source(args->grid, args->source_row, args->source_column, step);
        }

#ifndef SIN_BARRERA
        barrier_wait(args->barrier);
#endif

        /* Thread 0 publishes step / snapshot */
        if (args->thread_id == 0) {
            int do_snap = (args->k > 0 && (step % args->k == 0));

            if (args->state != NULL) {
                if (do_snap) {
                    published_state_publish_snapshot(args->state, step, args->grid->Ez);
                } else {
                    published_state_update_step(args->state, step);
                }
            }

            if (args->snapshot_queue != NULL && do_snap) {
                snapshot_t *snap = snapshot_create(args->job_id, args->grid->nx,
                                                   args->grid->ny, step, args->grid->Ez);
                if (snap != NULL) {
                    bqueue_push(args->snapshot_queue, snap);
                }
            }
        }
    }

    return NULL;
}

int fdtd_run_extended(fdtdGrid *grid, int total_steps, int thread_count,
                      int source_row, int source_column,
                      int k, unsigned long job_id,
                      published_state_t *state,
                      bqueue_t *snapshot_queue) {
    if (grid == NULL || thread_count <= 0 || total_steps <= 0) {
        return -1;
    }

    barrier_t barrier;
    if (barrier_init(&barrier, thread_count) != 0) {
        return -1;
    }

    pthread_t *threads = malloc((size_t)thread_count * sizeof(pthread_t));
    solver_thread_args_t *argsArray = malloc((size_t)thread_count * sizeof(solver_thread_args_t));
    if (threads == NULL || argsArray == NULL) {
        free(threads);
        free(argsArray);
        barrier_destroy(&barrier);
        return -1;
    }

    int quotient = grid->nx / thread_count;
    int remainder = grid->nx % thread_count;
    int threads_created = 0;

    for (int i = 0; i < thread_count; i++) {
        int extra_rows = (i < remainder) ? i : remainder;
        int start = i * quotient + extra_rows;
        int end = start + quotient + ((i < remainder) ? 1 : 0);

        argsArray[i].row_start = start;
        argsArray[i].row_end = end;
        argsArray[i].thread_id = i;
        argsArray[i].grid = grid;
        argsArray[i].total_steps = total_steps;
        argsArray[i].source_column = source_column;
        argsArray[i].source_row = source_row;
        argsArray[i].barrier = &barrier;
        argsArray[i].k = k;
        argsArray[i].job_id = job_id;
        argsArray[i].state = state;
        argsArray[i].snapshot_queue = snapshot_queue;

        if (pthread_create(&threads[i], NULL, solver_thread, &argsArray[i]) != 0) {
            break;
        }
        threads_created++;
    }

    for (int i = 0; i < threads_created; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(argsArray);
    barrier_destroy(&barrier);

    return (threads_created == thread_count) ? 0 : -1;
}

int fdtd_run(fdtdGrid *grid, int total_steps, int thread_count,
             int source_row, int source_column) {
    return fdtd_run_extended(grid, total_steps, thread_count,
                             source_row, source_column,
                             0, 0, NULL, NULL);
}
