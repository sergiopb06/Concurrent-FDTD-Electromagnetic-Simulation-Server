#define _POSIX_C_SOURCE 200809L

#include "../includes/bqueue.h"
#include <stdlib.h>
#include <errno.h>

static int sem_wait_retry(sem_t *sem) {
    while (sem_wait(sem) < 0) {
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}

int bqueue_init(bqueue_t *q, size_t capacity) {
    if (q == NULL || capacity == 0) {
        return -1;
    }

    q->buffer = malloc(capacity * sizeof(void *));
    if (q->buffer == NULL) {
        return -1;
    }

    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->stopped = 0;

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        free(q->buffer);
        q->buffer = NULL;
        return -1;
    }

    if (sem_init(&q->slots, 0, (unsigned int)capacity) != 0) {
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        q->buffer = NULL;
        return -1;
    }

    if (sem_init(&q->items, 0, 0) != 0) {
        sem_destroy(&q->slots);
        pthread_mutex_destroy(&q->mutex);
        free(q->buffer);
        q->buffer = NULL;
        return -1;
    }

    return 0;
}

int bqueue_push(bqueue_t *q, void *item) {
    if (q == NULL) {
        return -1;
    }

    if (sem_wait_retry(&q->slots) < 0) {
        return -1;
    }

    pthread_mutex_lock(&q->mutex);
    if (q->stopped) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->slots);
        errno = ECANCELED;
        return -1;
    }

    q->buffer[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->items);

    return 0;
}

int bqueue_try_push(bqueue_t *q, void *item) {
    if (q == NULL) {
        return -1;
    }

    if (sem_trywait(&q->slots) != 0) {
        /* Queue is full (errno == EAGAIN) */
        return -1;
    }

    pthread_mutex_lock(&q->mutex);
    if (q->stopped) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->slots);
        errno = ECANCELED;
        return -1;
    }

    q->buffer[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->items);

    return 0;
}

int bqueue_pop(bqueue_t *q, void **out_item) {
    if (q == NULL || out_item == NULL) {
        return -1;
    }

    if (sem_wait_retry(&q->items) < 0) {
        *out_item = NULL;
        return -1;
    }

    pthread_mutex_lock(&q->mutex);

    /* If queue was stopped and no items left */
    if (q->count == 0 && q->stopped) {
        pthread_mutex_unlock(&q->mutex);
        /* Re-post to unblock other waiting consumers if any */
        sem_post(&q->items);
        *out_item = NULL;
        return -1;
    }

    *out_item = q->buffer[q->head];
    q->buffer[q->head] = NULL;
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->slots);

    return 0;
}

int bqueue_stop(bqueue_t *q) {
    if (q == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->mutex);
    q->stopped = 1;
    pthread_mutex_unlock(&q->mutex);

    /* Unblock any threads waiting in pop or push */
    sem_post(&q->items);
    sem_post(&q->slots);

    return 0;
}

size_t bqueue_size(bqueue_t *q) {
    if (q == NULL) {
        return 0;
    }
    pthread_mutex_lock(&q->mutex);
    size_t sz = q->count;
    pthread_mutex_unlock(&q->mutex);
    return sz;
}

int bqueue_destroy(bqueue_t *q) {
    if (q == NULL) {
        return -1;
    }

    bqueue_stop(q);

    sem_destroy(&q->items);
    sem_destroy(&q->slots);
    pthread_mutex_destroy(&q->mutex);

    free(q->buffer);
    q->buffer = NULL;
    q->capacity = 0;
    q->head = 0;
    q->tail = 0;
    q->count = 0;

    return 0;
}
