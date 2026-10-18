#define _POSIX_C_SOURCE 200809L

#include "../includes/state.h"
#include <stdlib.h>
#include <string.h>

int published_state_init(published_state_t *ps) {
    if (ps == NULL) {
        return -1;
    }

    memset(ps, 0, sizeof(*ps));
    if (rwlock_init(&ps->rwlock) != 0) {
        return -1;
    }

    return 0;
}

int published_state_start_job(published_state_t *ps, unsigned long job_id,
                              int total_steps, int nx, int ny) {
    if (ps == NULL || nx <= 0 || ny <= 0) {
        return -1;
    }

    if (rwlock_wrlock(&ps->rwlock) != 0) {
        return -1;
    }

    ps->job_id = job_id;
    ps->current_step = 0;
    ps->total_steps = total_steps;
    ps->nx = nx;
    ps->ny = ny;
    ps->snapshot_step = -1;
    ps->is_running = 1;

    size_t needed_cells = (size_t)nx * (size_t)ny;
    if (ps->snapshot_cells < needed_cells) {
        free(ps->snapshot_ez);
        ps->snapshot_ez = malloc(needed_cells * sizeof(double));
        if (ps->snapshot_ez != NULL) {
            ps->snapshot_cells = needed_cells;
        } else {
            ps->snapshot_cells = 0;
        }
    }

    if (ps->snapshot_ez != NULL) {
        memset(ps->snapshot_ez, 0, needed_cells * sizeof(double));
    }

    rwlock_wrunlock(&ps->rwlock);
    return 0;
}

int published_state_update_step(published_state_t *ps, int step) {
    if (ps == NULL) {
        return -1;
    }

    if (rwlock_wrlock(&ps->rwlock) != 0) {
        return -1;
    }

    ps->current_step = step;

    rwlock_wrunlock(&ps->rwlock);
    return 0;
}

int published_state_publish_snapshot(published_state_t *ps, int step, const double *ez) {
    if (ps == NULL || ez == NULL) {
        return -1;
    }

    if (rwlock_wrlock(&ps->rwlock) != 0) {
        return -1;
    }

    ps->current_step = step;
    ps->snapshot_step = step;

    if (ps->snapshot_ez != NULL) {
        size_t cells = (size_t)ps->nx * (size_t)ps->ny;
        memcpy(ps->snapshot_ez, ez, cells * sizeof(double));
    }

    rwlock_wrunlock(&ps->rwlock);
    return 0;
}

int published_state_finish_job(published_state_t *ps) {
    if (ps == NULL) {
        return -1;
    }

    if (rwlock_wrlock(&ps->rwlock) != 0) {
        return -1;
    }

    ps->is_running = 0;

    rwlock_wrunlock(&ps->rwlock);
    return 0;
}

int published_state_get_status(published_state_t *ps, unsigned long job_id,
                               int *out_step, int *out_total) {
    if (ps == NULL || out_step == NULL || out_total == NULL) {
        return -1;
    }

    if (rwlock_rdlock(&ps->rwlock) != 0) {
        return -1;
    }

    int rc = -1;
    if (ps->is_running && ps->job_id == job_id) {
        *out_step = ps->current_step;
        *out_total = ps->total_steps;
        rc = 0;
    }

    rwlock_rdunlock(&ps->rwlock);
    return rc;
}

int published_state_get_peek(published_state_t *ps, unsigned long job_id,
                             int i, int j, int *out_step, double *out_val) {
    if (ps == NULL || out_step == NULL || out_val == NULL) {
        return -1;
    }

    if (rwlock_rdlock(&ps->rwlock) != 0) {
        return -1;
    }

    int rc = -1;
    if (ps->job_id != job_id) {
        rc = -1; /* snapshot belongs to another job (or none) */
    } else if (i < 0 || i >= ps->nx || j < 0 || j >= ps->ny) {
        rc = -2; /* Coordinates out of bounds */
    } else if (ps->snapshot_ez == NULL || ps->snapshot_step < 0) {
        rc = -3; /* No snapshot yet */
    } else {
        *out_step = ps->snapshot_step;
        *out_val = ps->snapshot_ez[i * ps->ny + j];
        rc = 0;
    }

    rwlock_rdunlock(&ps->rwlock);
    return rc;
}

void published_state_destroy(published_state_t *ps) {
    if (ps == NULL) {
        return;
    }

    rwlock_wrlock(&ps->rwlock);
    free(ps->snapshot_ez);
    ps->snapshot_ez = NULL;
    ps->snapshot_cells = 0;
    ps->is_running = 0;
    rwlock_wrunlock(&ps->rwlock);

    rwlock_destroy(&ps->rwlock);
}
