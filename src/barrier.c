#define _POSIX_C_SOURCE 200809L

#include "../includes/barrier.h"
#include <errno.h>
#include <stddef.h>

static int sem_wait_retry(sem_t *sem) {
    while (sem_wait(sem) < 0) {
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}

int barrier_init(barrier_t *b, int count) {
    if (b == NULL || count <= 0) {
        return -1;
    }

    b->count = 0;
    b->total = count;

    if (pthread_mutex_init(&b->mutex, NULL) != 0) {
        return -1;
    }

    if (sem_init(&b->turnstile1, 0, 0) != 0) {
        pthread_mutex_destroy(&b->mutex);
        return -1;
    }

    if (sem_init(&b->turnstile2, 0, 1) != 0) {
        sem_destroy(&b->turnstile1);
        pthread_mutex_destroy(&b->mutex);
        return -1;
    }

    return 0;
}

int barrier_wait(barrier_t *b) {
#ifdef SIN_BARRERA
    (void)b;
    return 0;
#else
    if (b == NULL) {
        return -1;
    }

    if (b->total <= 1) {
        return 0;
    }

    /* Phase 1: wait for all threads to arrive */
    pthread_mutex_lock(&b->mutex);
    b->count++;
    if (b->count == b->total) {
        sem_wait_retry(&b->turnstile2); /* lock second turnstile */
        sem_post(&b->turnstile1);        /* unlock first turnstile */
    }
    pthread_mutex_unlock(&b->mutex);

    sem_wait_retry(&b->turnstile1);
    sem_post(&b->turnstile1);

    /* Critical point between phases */

    /* Phase 2: wait for all threads to exit */
    pthread_mutex_lock(&b->mutex);
    b->count--;
    if (b->count == 0) {
        sem_wait_retry(&b->turnstile1); /* lock first turnstile */
        sem_post(&b->turnstile2);        /* unlock second turnstile */
    }
    pthread_mutex_unlock(&b->mutex);

    sem_wait_retry(&b->turnstile2);
    sem_post(&b->turnstile2);

    return 0;
#endif
}

int barrier_destroy(barrier_t *b) {
    if (b == NULL) {
        return -1;
    }

    sem_destroy(&b->turnstile1);
    sem_destroy(&b->turnstile2);
    pthread_mutex_destroy(&b->mutex);
    return 0;
}
