#define _POSIX_C_SOURCE 200809L

#include "../includes/snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

snapshot_t *snapshot_create(unsigned long job_id, int nx, int ny, int step, const double *ez) {
    if (nx <= 0 || ny <= 0 || ez == NULL) {
        return NULL;
    }

    snapshot_t *snap = malloc(sizeof(snapshot_t));
    if (snap == NULL) {
        return NULL;
    }

    size_t total_cells = (size_t)nx * (size_t)ny;
    snap->data = malloc(total_cells * sizeof(double));
    if (snap->data == NULL) {
        free(snap);
        return NULL;
    }

    memcpy(snap->data, ez, total_cells * sizeof(double));
    snap->job_id = job_id;
    snap->nx = (int32_t)nx;
    snap->ny = (int32_t)ny;
    snap->step = (int32_t)step;

    return snap;
}

void snapshot_free(snapshot_t *snap) {
    if (snap == NULL) {
        return;
    }
    free(snap->data);
    snap->data = NULL;
    free(snap);
}

int snapshot_write_file(const snapshot_t *snap) {
    if (snap == NULL || snap->data == NULL) {
        return -1;
    }

    char filename[128];
    int len = snprintf(filename, sizeof(filename), "job%lu_step%d.bin",
                       snap->job_id, (int)snap->step);
    if (len < 0 || (size_t)len >= sizeof(filename)) {
        return -1;
    }

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("fopen snapshot");
        return -1;
    }

    uint32_t header[3];
    header[0] = (uint32_t)snap->nx;
    header[1] = (uint32_t)snap->ny;
    header[2] = (uint32_t)snap->step;

    if (fwrite(header, sizeof(uint32_t), 3, fp) != 3) {
        fclose(fp);
        return -1;
    }

    size_t total_cells = (size_t)snap->nx * (size_t)snap->ny;
    if (fwrite(snap->data, sizeof(double), total_cells, fp) != total_cells) {
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        return -1;
    }

    return 0;
}

void *snapshot_writer_thread(void *arg) {
    bqueue_t *queue = (bqueue_t *)arg;
    if (queue == NULL) {
        return NULL;
    }

    void *item = NULL;
    while (bqueue_pop(queue, &item) == 0) {
        if (item == NULL) {
            break;
        }
        snapshot_t *snap = (snapshot_t *)item;
        (void)snapshot_write_file(snap);
        snapshot_free(snap);
        item = NULL;
    }

    return NULL;
}
