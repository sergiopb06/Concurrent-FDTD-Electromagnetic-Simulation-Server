#ifndef BARRIER_H
#define BARRIER_H

#include <pthread.h>
#include <semaphore.h>

typedef struct {
    int count;
    int total;
    pthread_mutex_t mutex;
    sem_t turnstile1;
    sem_t turnstile2;
} barrier_t;

int barrier_init(barrier_t *b, int count);
int barrier_wait(barrier_t *b);
int barrier_destroy(barrier_t *b);

#endif /* BARRIER_H */
