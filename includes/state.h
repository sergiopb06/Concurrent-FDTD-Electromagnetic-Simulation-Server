#ifndef STATE_H
#define STATE_H

#include "rwlock.h"
#include <stddef.h>

typedef struct {
    rwlock_t rwlock;
    unsigned long job_id;
    int current_step;
    int total_steps;
    int nx;
    int ny;
    int snapshot_step;
    double *snapshot_ez;
    size_t snapshot_cells;
    int is_running;
} published_state_t;

int published_state_init(published_state_t *ps);
int published_state_start_job(published_state_t *ps, unsigned long job_id,
                              int total_steps, int nx, int ny);
int published_state_update_step(published_state_t *ps, int step);
int published_state_publish_snapshot(published_state_t *ps, int step, const double *ez);
int published_state_finish_job(published_state_t *ps);
int published_state_get_status(published_state_t *ps, unsigned long job_id,
                               int *out_step, int *out_total);
int published_state_get_peek(published_state_t *ps, unsigned long job_id,
                             int i, int j, int *out_step, double *out_val);
void published_state_destroy(published_state_t *ps);

#endif /* STATE_H */
