#define _POSIX_C_SOURCE 200809L

#include "../includes/rwlock.h"
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

int rwlock_init(rwlock_t *rw) {
    if (rw == NULL) {
        return -1;
    }
    rw->readers = 0;

    if (pthread_mutex_init(&rw->read_mutex, NULL) != 0) {
        return -1;
    }

    if (sem_init(&rw->turnstile, 0, 1) != 0) {
        pthread_mutex_destroy(&rw->read_mutex);
        return -1;
    }

    if (sem_init(&rw->room_empty, 0, 1) != 0) {
        sem_destroy(&rw->turnstile);
        pthread_mutex_destroy(&rw->read_mutex);
        return -1;
    }

    return 0;
}

int rwlock_rdlock(rwlock_t *rw) {
    if (rw == NULL) {
        return -1;
    }

    /* Esperar si hay un escritor */
    if (sem_wait_retry(&rw->turnstile) < 0) {
        return -1;
    }
    if (sem_post(&rw->turnstile) < 0) {
        return -1;
    }

    pthread_mutex_lock(&rw->read_mutex);
    rw->readers++;
    if (rw->readers == 1) {
        if (sem_wait_retry(&rw->room_empty) < 0) {
            rw->readers--;
            pthread_mutex_unlock(&rw->read_mutex);
            return -1;
        }
    }
    pthread_mutex_unlock(&rw->read_mutex);

    return 0;
}

int rwlock_rdunlock(rwlock_t *rw) {
    if (rw == NULL) {
        return -1;
    }

    pthread_mutex_lock(&rw->read_mutex);
    rw->readers--;
    if (rw->readers == 0) {
        sem_post(&rw->room_empty);
    }
    pthread_mutex_unlock(&rw->read_mutex);

    return 0;
}

int rwlock_wrlock(rwlock_t *rw) {
    if (rw == NULL) {
        return -1;
    }

    if (sem_wait_retry(&rw->turnstile) < 0) {
        return -1;
    }

    /* Espera a que salgan los lectores activos */
    if (sem_wait_retry(&rw->room_empty) < 0) {
        sem_post(&rw->turnstile);
        return -1;
    }

    return 0;
}

int rwlock_wrunlock(rwlock_t *rw) {
    if (rw == NULL) {
        return -1;
    }

    sem_post(&rw->room_empty);
    sem_post(&rw->turnstile);

    return 0;
}

int rwlock_destroy(rwlock_t *rw) {
    if (rw == NULL) {
        return -1;
    }

    sem_destroy(&rw->room_empty);
    sem_destroy(&rw->turnstile);
    pthread_mutex_destroy(&rw->read_mutex);

    return 0;
}
