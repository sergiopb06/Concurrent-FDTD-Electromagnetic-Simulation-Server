#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include "bqueue.h"

typedef struct {
    unsigned long job_id;
    int32_t nx;
    int32_t ny;
    int32_t step;
    double *data;
} snapshot_t;

snapshot_t *snapshot_create(unsigned long job_id, int nx, int ny, int step, const double *ez);
void snapshot_free(snapshot_t *snap);
int snapshot_write_file(const snapshot_t *snap);
void *snapshot_writer_thread(void *arg);

#endif /* SNAPSHOT_H */
