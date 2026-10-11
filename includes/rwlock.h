#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>
#include <semaphore.h>

typedef struct {
    int readers;
    pthread_mutex_t read_mutex;
    sem_t turnstile;
    sem_t room_empty;
} rwlock_t;

int rwlock_init(rwlock_t *rw);
int rwlock_rdlock(rwlock_t *rw);
int rwlock_rdunlock(rwlock_t *rw);
int rwlock_wrlock(rwlock_t *rw);
int rwlock_wrunlock(rwlock_t *rw);
int rwlock_destroy(rwlock_t *rw);

#endif /* RWLOCK_H */
