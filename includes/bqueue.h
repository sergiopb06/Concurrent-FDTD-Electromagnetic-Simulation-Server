#ifndef BQUEUE_H
#define BQUEUE_H

#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>

typedef struct {
    void **buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    int stopped;
    pthread_mutex_t mutex;
    sem_t slots;
    sem_t items;
} bqueue_t;

int bqueue_init(bqueue_t *q, size_t capacity);
int bqueue_push(bqueue_t *q, void *item);
int bqueue_try_push(bqueue_t *q, void *item);
int bqueue_pop(bqueue_t *q, void **out_item);
int bqueue_stop(bqueue_t *q);
size_t bqueue_size(bqueue_t *q);
int bqueue_destroy(bqueue_t *q);

#endif /* BQUEUE_H */
