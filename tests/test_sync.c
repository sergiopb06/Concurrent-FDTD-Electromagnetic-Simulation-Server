#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <stdint.h>

#include "../includes/barrier.h"
#include "../includes/rwlock.h"
#include "../includes/bqueue.h"
#include "../includes/snapshot.h"
#include "../includes/state.h"

static int g_fails = 0;
static int g_total = 0;

#define CHECK(condition, message)                                           \
    do {                                                                    \
        g_total++;                                                          \
        if (condition) {                                                    \
            printf("  [\033[32mPASS\033[0m] %s\n", message);               \
        } else {                                                            \
            printf("  [\033[31mFAIL\033[0m] %s (line %d)\n", message, __LINE__); \
            g_fails++;                                                      \
        }                                                                   \
    } while (0)

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ========================================================================= */
/* 1. PRUEBAS DE BARRERA REUTILIZABLE (barrier.h / barrier.c)               */
/* ========================================================================= */

#define BARRIER_THREADS 8
#define BARRIER_ROUNDS 100

typedef struct {
    int thread_id;
    int num_threads;
    int total_rounds;
    barrier_t *barrier;
    int *shared_rounds;
    int *violation_count;
    int add_delays;
} barrier_worker_args_t;

static void *barrier_worker(void *arg) {
    barrier_worker_args_t *args = (barrier_worker_args_t *)arg;
    int id = args->thread_id;
    int total = args->num_threads;

    for (int r = 0; r < args->total_rounds; r++) {
        /* Registra la ronda antes de cruzar */
        args->shared_rounds[id] = r;

        if (args->add_delays) {
            /* Asimetria intencional: hilos pares tardan mas */
            if (id % 2 == 0) {
                usleep(150);
            }
        }

        /* Primera barrera: fase de entrada/cómputo */
        barrier_wait(args->barrier);

        /* Invariante fundamental: al salir de la barrera de la ronda r,
           ningun hilo puede estar todavia en una ronda anterior a r */
        for (int k = 0; k < total; k++) {
            if (args->shared_rounds[k] < r) {
                __atomic_fetch_add(args->violation_count, 1, __ATOMIC_RELAXED);
            }
        }

        /* Segunda barrera del paso (como en FDTD fase a y fase b) */
        barrier_wait(args->barrier);

        /* Validacion tras la segunda barrera: ningun hilo se adelanto a r+2 */
        for (int k = 0; k < total; k++) {
            if (args->shared_rounds[k] > r + 1) {
                __atomic_fetch_add(args->violation_count, 1, __ATOMIC_RELAXED);
            }
        }
    }

    return NULL;
}

/* Prueba 1.1: N hilos x M rondas con verificacion estricta de orden de paso */
static void test_barrier_n_threads_m_rounds(void) {
    printf("\n=== Barrera: %d hilos x %d rondas (Downey 3.7 dos torniquetes) ===\n",
           BARRIER_THREADS, BARRIER_ROUNDS);

    barrier_t barrier;
    CHECK(barrier_init(&barrier, BARRIER_THREADS) == 0, "barrier_init retorna 0");

    int shared_rounds[BARRIER_THREADS];
    memset(shared_rounds, 0, sizeof(shared_rounds));
    int violations = 0;

    pthread_t th[BARRIER_THREADS];
    barrier_worker_args_t args[BARRIER_THREADS];

    for (int i = 0; i < BARRIER_THREADS; i++) {
        args[i].thread_id = i;
        args[i].num_threads = BARRIER_THREADS;
        args[i].total_rounds = BARRIER_ROUNDS;
        args[i].barrier = &barrier;
        args[i].shared_rounds = shared_rounds;
        args[i].violation_count = &violations;
        args[i].add_delays = 0;
        pthread_create(&th[i], NULL, barrier_worker, &args[i]);
    }

    for (int i = 0; i < BARRIER_THREADS; i++) {
        pthread_join(th[i], NULL);
    }

    CHECK(violations == 0, "Ningun hilo entro a la ronda r+1 antes de que todos completaran la r");
    for (int i = 0; i < BARRIER_THREADS; i++) {
        CHECK(shared_rounds[i] == BARRIER_ROUNDS - 1, "Todos los hilos llegaron exactamente a la ultima ronda");
    }

    CHECK(barrier_destroy(&barrier) == 0, "barrier_destroy retorna 0");
}

/* Prueba 1.2: Asimetria extrema de tiempos (fast vs slow threads) */
static void test_barrier_asymmetric_delays(void) {
    printf("\n=== Barrera: Asimetria de velocidad (fast vs slow threads) ===\n");

    const int threads = 6;
    const int rounds = 40;
    barrier_t barrier;
    barrier_init(&barrier, threads);

    int shared_rounds[6] = {0};
    int violations = 0;
    pthread_t th[6];
    barrier_worker_args_t args[6];

    for (int i = 0; i < threads; i++) {
        args[i].thread_id = i;
        args[i].num_threads = threads;
        args[i].total_rounds = rounds;
        args[i].barrier = &barrier;
        args[i].shared_rounds = shared_rounds;
        args[i].violation_count = &violations;
        args[i].add_delays = 1;
        pthread_create(&th[i], NULL, barrier_worker, &args[i]);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(th[i], NULL);
    }

    CHECK(violations == 0, "Dos torniquetes impiden que hilos rapidos adelanten a hilos lentos rezagados");
    barrier_destroy(&barrier);
}

/* Prueba 1.3: Concurrencia masiva (32 hilos x 200 rondas) */
static void test_barrier_high_contention(void) {
    printf("\n=== Barrera: Estres con alta concurrencia (32 hilos) ===\n");

    const int threads = 32;
    const int rounds = 150;
    barrier_t barrier;
    barrier_init(&barrier, threads);

    int shared_rounds[32] = {0};
    int violations = 0;
    pthread_t th[32];
    barrier_worker_args_t args[32];

    for (int i = 0; i < threads; i++) {
        args[i].thread_id = i;
        args[i].num_threads = threads;
        args[i].total_rounds = rounds;
        args[i].barrier = &barrier;
        args[i].shared_rounds = shared_rounds;
        args[i].violation_count = &violations;
        args[i].add_delays = 0;
        pthread_create(&th[i], NULL, barrier_worker, &args[i]);
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(th[i], NULL);
    }

    CHECK(violations == 0, "Barrera estable bajo carga pesada de 32 hilos");
    barrier_destroy(&barrier);
}

/* Prueba 1.4: Caso degenerado con 1 hilo */
static void test_barrier_single_thread(void) {
    printf("\n=== Barrera: Caso degenerado de 1 hilo ===\n");

    barrier_t b;
    CHECK(barrier_init(&b, 1) == 0, "barrier_init(count=1)");
    for (int i = 0; i < 20; i++) {
        CHECK(barrier_wait(&b) == 0, "barrier_wait avanza inmediatamente con 1 hilo");
    }
    CHECK(barrier_destroy(&b) == 0, "barrier_destroy con 1 hilo");
}

/* ========================================================================= */
/* 2. PRUEBAS DE CANDADO LECTORES-ESCRITOR (rwlock.h / rwlock.c)             */
/* ========================================================================= */

typedef struct {
    rwlock_t *rw;
    int *active_readers;
    int *max_readers;
    int *active_writers;
    int *violations;
    pthread_mutex_t *stat_mutex;
    int iterations;
} rwlock_test_ctx_t;

static void *reader_overlap_worker(void *arg) {
    rwlock_test_ctx_t *ctx = (rwlock_test_ctx_t *)arg;

    for (int i = 0; i < ctx->iterations; i++) {
        rwlock_rdlock(ctx->rw);

        pthread_mutex_lock(ctx->stat_mutex);
        (*ctx->active_readers)++;
        if (*ctx->active_readers > *ctx->max_readers) {
            *ctx->max_readers = *ctx->active_readers;
        }
        if (*ctx->active_writers > 0) {
            (*ctx->violations)++;
        }
        pthread_mutex_unlock(ctx->stat_mutex);

        /* Mantenerse un instante en la seccion critica */
        usleep(1500);

        pthread_mutex_lock(ctx->stat_mutex);
        (*ctx->active_readers)--;
        pthread_mutex_unlock(ctx->stat_mutex);

        rwlock_rdunlock(ctx->rw);
        usleep(500);
    }

    return NULL;
}

/* Prueba 2.1: Multiples lectores simultaneos */
static void test_rwlock_simultaneous_readers(void) {
    printf("\n=== L-E: Lectores simultaneos concurrentes ===\n");

    rwlock_t rw;
    rwlock_init(&rw);

    pthread_mutex_t stat_mutex = PTHREAD_MUTEX_INITIALIZER;
    int active_readers = 0;
    int max_readers = 0;
    int active_writers = 0;
    int violations = 0;

    const int num_readers = 10;
    pthread_t th[10];
    rwlock_test_ctx_t ctx = {
        .rw = &rw,
        .active_readers = &active_readers,
        .max_readers = &max_readers,
        .active_writers = &active_writers,
        .violations = &violations,
        .stat_mutex = &stat_mutex,
        .iterations = 15
    };

    for (int i = 0; i < num_readers; i++) {
        pthread_create(&th[i], NULL, reader_overlap_worker, &ctx);
    }

    for (int i = 0; i < num_readers; i++) {
        pthread_join(th[i], NULL);
    }

    CHECK(max_readers >= 3, "Varios lectores leen simultaneamente en seccion critica");
    CHECK(violations == 0, "Cero violaciones durante lectura concurrente");
    printf("    (Maximo de lectores concurrentes observado: %d de %d)\n", max_readers, num_readers);

    rwlock_destroy(&rw);
    pthread_mutex_destroy(&stat_mutex);
}

/* Prueba 2.2: Exclusion estricta del escritor */
typedef struct {
    rwlock_t *rw;
    volatile int *stop;
    int *active_readers;
    int *active_writers;
    int *violations;
} rw_exclusion_ctx_t;

static void *exclusion_reader(void *arg) {
    rw_exclusion_ctx_t *ctx = (rw_exclusion_ctx_t *)arg;
    while (!*ctx->stop) {
        rwlock_rdlock(ctx->rw);
        int w = __atomic_load_n(ctx->active_writers, __ATOMIC_SEQ_CST);
        if (w > 0) {
            __atomic_fetch_add(ctx->violations, 1, __ATOMIC_SEQ_CST);
        }
        __atomic_fetch_add(ctx->active_readers, 1, __ATOMIC_SEQ_CST);
        usleep(100);
        __atomic_fetch_sub(ctx->active_readers, 1, __ATOMIC_SEQ_CST);
        rwlock_rdunlock(ctx->rw);
        usleep(50);
    }
    return NULL;
}

static void *exclusion_writer(void *arg) {
    rw_exclusion_ctx_t *ctx = (rw_exclusion_ctx_t *)arg;
    for (int i = 0; i < 30; i++) {
        rwlock_wrlock(ctx->rw);
        int prev_w = __atomic_fetch_add(ctx->active_writers, 1, __ATOMIC_SEQ_CST);
        int r = __atomic_load_n(ctx->active_readers, __ATOMIC_SEQ_CST);
        if (prev_w > 0 || r > 0) {
            __atomic_fetch_add(ctx->violations, 1, __ATOMIC_SEQ_CST);
        }
        usleep(500);
        __atomic_fetch_sub(ctx->active_writers, 1, __ATOMIC_SEQ_CST);
        rwlock_wrunlock(ctx->rw);
        usleep(200);
    }
    return NULL;
}

static void test_rwlock_writer_exclusion(void) {
    printf("\n=== L-E: Exclusion mutua estricta de escritor ===\n");

    rwlock_t rw;
    rwlock_init(&rw);

    volatile int stop = 0;
    int active_readers = 0;
    int active_writers = 0;
    int violations = 0;

    rw_exclusion_ctx_t ctx = {
        .rw = &rw,
        .stop = &stop,
        .active_readers = &active_readers,
        .active_writers = &active_writers,
        .violations = &violations
    };

    pthread_t readers[6];
    pthread_t writers[3];

    for (int i = 0; i < 6; i++) {
        pthread_create(&readers[i], NULL, exclusion_reader, &ctx);
    }
    for (int i = 0; i < 3; i++) {
        pthread_create(&writers[i], NULL, exclusion_writer, &ctx);
    }

    for (int i = 0; i < 3; i++) {
        pthread_join(writers[i], NULL);
    }
    stop = 1;
    for (int i = 0; i < 6; i++) {
        pthread_join(readers[i], NULL);
    }

    CHECK(violations == 0, "Exclusion mutua perfecta: nunca hay escritor con lectores ni dos escritores");
    rwlock_destroy(&rw);
}

/* Prueba 2.3: Ausencia de inanicion del escritor (Downey 4.2.6) */
typedef struct {
    rwlock_t *rw;
    volatile int *stop;
    unsigned long *read_count;
} starvation_reader_ctx_t;

static void *flood_reader_worker(void *arg) {
    starvation_reader_ctx_t *ctx = (starvation_reader_ctx_t *)arg;
    while (!*ctx->stop) {
        rwlock_rdlock(ctx->rw);
        __atomic_fetch_add(ctx->read_count, 1, __ATOMIC_RELAXED);
        /* Breve permanencia para mantener siempre lectores adentro */
        usleep(300);
        rwlock_rdunlock(ctx->rw);
    }
    return NULL;
}

static void test_rwlock_writer_no_starvation(void) {
    printf("\n=== L-E: No inanicion del escritor (Downey 4.2.6) ===\n");

    rwlock_t rw;
    rwlock_init(&rw);

    volatile int stop = 0;
    unsigned long total_reads = 0;
    const int num_readers = 8;
    pthread_t readers[8];
    starvation_reader_ctx_t ctx = {
        .rw = &rw,
        .stop = &stop,
        .read_count = &total_reads
    };

    /* Iniciar flujo ininterrumpido de lectores */
    for (int i = 0; i < num_readers; i++) {
        pthread_create(&readers[i], NULL, flood_reader_worker, &ctx);
    }

    /* Esperar a que el flujo de lectores este completamente activo */
    usleep(20000);

    /* El escritor debe poder entrar sin esperar indefinidamente */
    double t_start = get_time_sec();
    CHECK(rwlock_wrlock(&rw) == 0, "Escritor solicita wrlock durante bombardeo de lectores");
    double elapsed = get_time_sec() - t_start;

    printf("    (Tiempo de espera del escritor: %.4f s con flujo continuo)\n", elapsed);
    CHECK(elapsed < 1.0, "El escritor entra oportunamente (< 1.0s); no sufre inanicion");

    /* Escritura segura */
    usleep(5000);
    rwlock_wrunlock(&rw);

    stop = 1;
    for (int i = 0; i < num_readers; i++) {
        pthread_join(readers[i], NULL);
    }

    printf("    (Total de lecturas realizadas durante la prueba: %lu)\n", total_reads);
    CHECK(total_reads > 50, "Flujo masivo de lectores verificado");

    rwlock_destroy(&rw);
}

/* Prueba 2.4: Integracion con published_state_t */
static void test_rwlock_published_state(void) {
    printf("\n=== L-E: Integracion con estado publicado (STATUS y PEEK) ===\n");

    published_state_t ps;
    CHECK(published_state_init(&ps) == 0, "published_state_init");

    CHECK(published_state_start_job(&ps, 101, 50, 8, 8) == 0, "start_job 101");

    double dummy_ez[64];
    for (int i = 0; i < 64; i++) {
        dummy_ez[i] = (double)i * 0.25;
    }

    CHECK(published_state_publish_snapshot(&ps, 20, dummy_ez) == 0, "publish_snapshot paso 20");

    int cur_step = 0, total_steps = 0;
    CHECK(published_state_get_status(&ps, 101, &cur_step, &total_steps) == 0, "get_status trabajo 101");
    CHECK(cur_step == 20 && total_steps == 50, "Valores de step y total correctos");

    int snap_step = 0;
    double val = 0.0;
    CHECK(published_state_get_peek(&ps, 101, 2, 3, &snap_step, &val) == 0, "get_peek celda (2,3)");
    CHECK(snap_step == 20, "peek reporta step 20");
    double expected_val = dummy_ez[2 * 8 + 3];
    CHECK(fabs(val - expected_val) < 1e-9, "peek entrega el valor exacto de la celda");

    /* Consulta de celda fuera de limites */
    CHECK(published_state_get_peek(&ps, 101, 10, 10, &snap_step, &val) == -2, "peek fuera de limites reporta -2");

    /* Trabajo desconocido */
    CHECK(published_state_get_status(&ps, 999, &cur_step, &total_steps) == -1, "status trabajo desconocido reporta -1");

    CHECK(published_state_finish_job(&ps) == 0, "finish_job");
    published_state_destroy(&ps);
}

/* ========================================================================= */
/* 3. PRUEBAS DE COLA ACOTADA (bqueue.h / bqueue.c)                         */
/* ========================================================================= */

/* Prueba 3.1: Llenar hasta capacidad, vaciar y orden FIFO */
static void test_bqueue_fifo_order(void) {
    printf("\n=== Cola: Llenar, vaciar y orden FIFO estricto ===\n");

    const size_t capacity = 8;
    bqueue_t q;
    CHECK(bqueue_init(&q, capacity) == 0, "bqueue_init(capacity=8)");

    /* Llenar */
    for (size_t i = 0; i < capacity; i++) {
        int rc = bqueue_push(&q, (void *)(uintptr_t)(i + 100));
        CHECK(rc == 0, "bqueue_push dentro de capacidad");
    }
    CHECK(bqueue_size(&q) == capacity, "bqueue_size igual a capacidad maxima");

    /* Vaciar y verificar orden FIFO */
    int fifo_ok = 1;
    for (size_t i = 0; i < capacity; i++) {
        void *item = NULL;
        bqueue_pop(&q, &item);
        uintptr_t val = (uintptr_t)item;
        if (val != (i + 100)) {
            fifo_ok = 0;
        }
    }
    CHECK(fifo_ok == 1, "Todos los elementos salieron en riguroso orden FIFO");
    CHECK(bqueue_size(&q) == 0, "bqueue_size es 0 tras vaciar");

    bqueue_destroy(&q);
}

/* Prueba 3.2: Rechazo BUSY al exceder capacidad con bqueue_try_push */
static void test_bqueue_try_push_busy(void) {
    printf("\n=== Cola: Semantica BUSY al exceder C con bqueue_try_push ===\n");

    const size_t cap = 4; /* Como las C sillas de la barberia */
    bqueue_t q;
    bqueue_init(&q, cap);

    /* Ocupar las 4 sillas */
    for (size_t i = 0; i < cap; i++) {
        CHECK(bqueue_try_push(&q, (void *)(uintptr_t)(i + 1)) == 0, "try_push silla disponible");
    }

    /* El 5to cliente llega: no hay sillas -> rechazo inmediato sin bloquear */
    int rc = bqueue_try_push(&q, (void *)(uintptr_t)999);
    CHECK(rc == -1, "try_push rechaza con -1 cuando la cola esta llena (BUSY)");
    CHECK(bqueue_size(&q) == cap, "Tamaño de cola se mantiene en 4");

    /* El barbero atiende un cliente (pop) */
    void *item = NULL;
    bqueue_pop(&q, &item);
    CHECK((uintptr_t)item == 1, "Primer cliente atendido");

    /* Ahora si hay espacio para el nuevo cliente */
    CHECK(bqueue_try_push(&q, (void *)(uintptr_t)999) == 0, "try_push acepta tras liberarse un espacio");

    bqueue_destroy(&q);
}

/* Prueba 3.3: Insercion bloqueante cuando el buffer esta lleno (bqueue_push) */
typedef struct {
    bqueue_t *q;
    int pushed;
} blocking_push_ctx_t;

static void *blocking_pusher(void *arg) {
    blocking_push_ctx_t *ctx = (blocking_push_ctx_t *)arg;
    bqueue_push(ctx->q, (void *)(uintptr_t)777);
    ctx->pushed = 1;
    return NULL;
}

static void test_bqueue_blocking_push(void) {
    printf("\n=== Cola: Insercion bloqueante del productor (bqueue_push) ===\n");

    const size_t cap = 2;
    bqueue_t q;
    bqueue_init(&q, cap);

    /* Llenar capacidad */
    bqueue_push(&q, (void *)(uintptr_t)1);
    bqueue_push(&q, (void *)(uintptr_t)2);

    blocking_push_ctx_t ctx = { .q = &q, .pushed = 0 };
    pthread_t pusher_th;
    pthread_create(&pusher_th, NULL, blocking_pusher, &ctx);

    /* Esperar un instante: el pusher debe seguir bloqueado */
    usleep(25000);
    CHECK(ctx.pushed == 0, "Productor se bloquea al intentar bqueue_push en cola llena");

    /* Consumir un elemento para liberar espacio */
    void *item = NULL;
    bqueue_pop(&q, &item);
    CHECK((uintptr_t)item == 1, "Consumidor extrae primer elemento");

    /* Ahora el pusher debe desbloquearse */
    pthread_join(pusher_th, NULL);
    CHECK(ctx.pushed == 1, "Productor se desbloquea exitosamente al haber espacio");

    bqueue_destroy(&q);
}

/* Prueba 3.4: Multiples productores y consumidores concurrentes (M:N) */
#define MP_PRODUCERS 4
#define MP_CONSUMERS 4
#define MP_ITEMS_PER_PRODUCER 250

typedef struct {
    bqueue_t *q;
    int producer_id;
} mp_producer_ctx_t;

typedef struct {
    bqueue_t *q;
    unsigned long *consumed_sum;
    int *consumed_count;
    pthread_mutex_t *mutex;
} mp_consumer_ctx_t;

static void *mp_producer(void *arg) {
    mp_producer_ctx_t *ctx = (mp_producer_ctx_t *)arg;
    for (int i = 1; i <= MP_ITEMS_PER_PRODUCER; i++) {
        uintptr_t val = (uintptr_t)(ctx->producer_id * 10000 + i);
        bqueue_push(ctx->q, (void *)val);
    }
    return NULL;
}

static void *mp_consumer(void *arg) {
    mp_consumer_ctx_t *ctx = (mp_consumer_ctx_t *)arg;
    while (1) {
        void *item = NULL;
        if (bqueue_pop(ctx->q, &item) != 0 || item == NULL) {
            break;
        }
        uintptr_t val = (uintptr_t)item;
        pthread_mutex_lock(ctx->mutex);
        (*ctx->consumed_sum) += (unsigned long)val;
        (*ctx->consumed_count)++;
        int done = (*ctx->consumed_count == MP_PRODUCERS * MP_ITEMS_PER_PRODUCER);
        pthread_mutex_unlock(ctx->mutex);

        if (done) {
            bqueue_stop(ctx->q);
            break;
        }
    }
    return NULL;
}

static void test_bqueue_multi_producer_multi_consumer(void) {
    printf("\n=== Cola: Concurrencia masiva M:N (4 productores, 4 consumidores) ===\n");

    bqueue_t q;
    bqueue_init(&q, 16);

    unsigned long consumed_sum = 0;
    int consumed_count = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t prod_th[MP_PRODUCERS];
    pthread_t cons_th[MP_CONSUMERS];
    mp_producer_ctx_t p_ctx[MP_PRODUCERS];
    mp_consumer_ctx_t c_ctx = {
        .q = &q,
        .consumed_sum = &consumed_sum,
        .consumed_count = &consumed_count,
        .mutex = &mutex
    };

    /* Suma esperada teorica */
    unsigned long expected_sum = 0;
    for (int p = 0; p < MP_PRODUCERS; p++) {
        for (int i = 1; i <= MP_ITEMS_PER_PRODUCER; i++) {
            expected_sum += (unsigned long)(p * 10000 + i);
        }
    }

    for (int i = 0; i < MP_CONSUMERS; i++) {
        pthread_create(&cons_th[i], NULL, mp_consumer, &c_ctx);
    }
    for (int i = 0; i < MP_PRODUCERS; i++) {
        p_ctx[i].q = &q;
        p_ctx[i].producer_id = i;
        pthread_create(&prod_th[i], NULL, mp_producer, &p_ctx[i]);
    }

    for (int i = 0; i < MP_PRODUCERS; i++) {
        pthread_join(prod_th[i], NULL);
    }
    for (int i = 0; i < MP_CONSUMERS; i++) {
        pthread_join(cons_th[i], NULL);
    }

    CHECK(consumed_count == MP_PRODUCERS * MP_ITEMS_PER_PRODUCER,
          "Todos los 1000 items fueron consumidos sin perdida ni duplicacion");
    CHECK(consumed_sum == expected_sum, "Suma de control exacta de todos los datos recibidos");

    bqueue_destroy(&q);
    pthread_mutex_destroy(&mutex);
}

/* Prueba 3.5: Parada segura (bqueue_stop) desbloquea hilos en espera */
static void *waiting_pop_worker(void *arg) {
    bqueue_t *q = (bqueue_t *)arg;
    void *item = NULL;
    int rc = bqueue_pop(q, &item);
    /* Al detener la cola debe retornar -1 y item == NULL */
    return (void *)(intptr_t)rc;
}

static void test_bqueue_stop_unblocks(void) {
    printf("\n=== Cola: Apagado limpio y despertar de hilos (bqueue_stop) ===\n");

    bqueue_t q;
    bqueue_init(&q, 4);

    pthread_t th[3];
    for (int i = 0; i < 3; i++) {
        pthread_create(&th[i], NULL, waiting_pop_worker, &q);
    }

    /* Asegurar que los hilos estan dormidos en pop */
    usleep(20000);

    /* Despertar a todos con bqueue_stop */
    CHECK(bqueue_stop(&q) == 0, "bqueue_stop ejecutado");

    for (int i = 0; i < 3; i++) {
        void *ret = NULL;
        pthread_join(th[i], &ret);
        CHECK((intptr_t)ret == -1, "Hilo en espera fue despertado limpiamente y retorno -1");
    }

    bqueue_destroy(&q);
}

/* ========================================================================= */
/* 4. PRUEBAS DE SNAPSHOT Y FORMATO BINARIO (snapshot.h / snapshot.c)       */
/* ========================================================================= */

/* Prueba 4.1: Formato binario de archivo job<id>_step<q>.bin */
static void test_snapshot_binary_file_format(void) {
    printf("\n=== Snapshot: Verificacion de formato de archivo binario ===\n");

    const int nx = 4;
    const int ny = 5;
    const int step = 42;
    const unsigned long job_id = 12345;

    double test_data[20];
    for (int i = 0; i < nx * ny; i++) {
        test_data[i] = (double)i * 1.5 + 0.125;
    }

    snapshot_t *snap = snapshot_create(job_id, nx, ny, step, test_data);
    CHECK(snap != NULL, "snapshot_create crea estructura correctamente");
    CHECK(snapshot_write_file(snap) == 0, "snapshot_write_file guarda a disco");
    snapshot_free(snap);

    /* Leer y validar contenido exacto del archivo */
    char filename[64];
    snprintf(filename, sizeof(filename), "job%lu_step%d.bin", job_id, step);

    FILE *fp = fopen(filename, "rb");
    CHECK(fp != NULL, "Archivo binario job12345_step42.bin existe en disco");

    uint32_t header[3] = {0};
    size_t read_hdr = fread(header, sizeof(uint32_t), 3, fp);
    CHECK(read_hdr == 3, "Cabecera contiene 3 enteros uint32_t");
    CHECK((int)header[0] == nx, "Cabecera nx coincide (4)");
    CHECK((int)header[1] == ny, "Cabecera ny coincide (5)");
    CHECK((int)header[2] == step, "Cabecera step coincide (42)");

    double read_data[20] = {0};
    size_t read_cells = fread(read_data, sizeof(double), nx * ny, fp);
    CHECK(read_cells == (size_t)(nx * ny), "Se leyeron exactamente nx*ny doubles");
    CHECK(memcmp(read_data, test_data, sizeof(test_data)) == 0,
          "Datos en archivo coinciden bit a bit con memoria");

    /* Validar que no hay bytes extra */
    fgetc(fp);
    CHECK(feof(fp), "Fin de archivo exacto (sin bytes residuales)");
    fclose(fp);

    /* Limpiar archivo de prueba */
    remove(filename);
}

/* Prueba 4.2: Hilo escritor consumiendo instantaneas de bqueue */
static void test_snapshot_writer_thread(void) {
    printf("\n=== Snapshot: Hilo escritor consumiendo de la cola acotada ===\n");

    bqueue_t snap_q;
    bqueue_init(&snap_q, 4);

    pthread_t writer_tid;
    CHECK(pthread_create(&writer_tid, NULL, snapshot_writer_thread, &snap_q) == 0,
          "Hilo escritor lanzado");

    const int nx = 4, ny = 4;
    double sample[16] = {1.0, 2.0, 3.0, 4.0};

    const unsigned long job_id = 99999;
    int test_steps[3] = {0, 10, 20};

    for (int i = 0; i < 3; i++) {
        snapshot_t *s = snapshot_create(job_id, nx, ny, test_steps[i], sample);
        bqueue_push(&snap_q, s);
    }

    /* Detener cola y esperar al hilo escritor */
    bqueue_stop(&snap_q);
    pthread_join(writer_tid, NULL);

    /* Verificar que los 3 archivos fueron generados y limpiarlos */
    int all_files_ok = 1;
    for (int i = 0; i < 3; i++) {
        char fn[64];
        snprintf(fn, sizeof(fn), "job%lu_step%d.bin", job_id, test_steps[i]);
        FILE *f = fopen(fn, "rb");
        if (f) {
            fclose(f);
            remove(fn);
        } else {
            all_files_ok = 0;
        }
    }
    CHECK(all_files_ok == 1, "Todos los archivos de instantaneas fueron escritos por el hilo consumidor");

    bqueue_destroy(&snap_q);
}

/* ========================================================================= */
/* MAIN RUNNER                                                               */
/* ========================================================================= */

int main(void) {
    printf("===============================================================\n");
    printf("   PRUEBAS DE MECANISMOS DE SINCRONIZACION (PERSONA A)\n");
    printf("===============================================================\n");

    /* 1. Barrera */
    test_barrier_n_threads_m_rounds();
    test_barrier_asymmetric_delays();
    test_barrier_high_contention();
    test_barrier_single_thread();

    /* 2. Candado Lectores-Escritor */
    test_rwlock_simultaneous_readers();
    test_rwlock_writer_exclusion();
    test_rwlock_writer_no_starvation();
    test_rwlock_published_state();

    /* 3. Cola Acotada */
    test_bqueue_fifo_order();
    test_bqueue_try_push_busy();
    test_bqueue_blocking_push();
    test_bqueue_multi_producer_multi_consumer();
    test_bqueue_stop_unblocks();

    /* 4. Snapshot */
    test_snapshot_binary_file_format();
    test_snapshot_writer_thread();

    printf("\n===============================================================\n");
    printf("RESUMEN DE PRUEBAS DE SINCRONIZACION:\n");
    printf("  Total de aserciones: %d\n", g_total);
    printf("  Exitosas:            \033[32m%d\033[0m\n", g_total - g_fails);
    printf("  Fallidas:            \033[31m%d\033[0m\n", g_fails);
    printf("===============================================================\n");

    return (g_fails == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
