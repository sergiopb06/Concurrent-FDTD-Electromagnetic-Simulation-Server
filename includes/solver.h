#ifndef SOLVER_H
#define SOLVER_H

#include "fdtd.h"
#include "state.h"
#include "bqueue.h"

int fdtd_run(fdtdGrid *grid, int total_steps, int thread_count,
             int source_row, int source_column);

int fdtd_run_extended(fdtdGrid *grid, int total_steps, int thread_count,
                      int source_row, int source_column,
                      int k, unsigned long job_id,
                      published_state_t *state,
                      bqueue_t *snapshot_queue);

#endif /* SOLVER_H */