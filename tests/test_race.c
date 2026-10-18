#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/*
 * Prueba de condición de carrera para demostrar la necesidad de sincronización.
 * Incremento de una variable compartida sin mutex.
 */

static int shared_counter = 0;

static void *race_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 10000; i++) {
        int temp = shared_counter;
        usleep(1); // Forzar cambio de contexto
        shared_counter = temp + 1;
    }
    return NULL;
}

int main(void) {
    printf("=== PRUEBA: Condición de carrera (sin sincronización) ===\n");
    
    pthread_t th1, th2;
    pthread_create(&th1, NULL, race_worker, NULL);
    pthread_create(&th2, NULL, race_worker, NULL);
    
    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    
    printf("Resultado esperado: 20000\n");
    printf("Resultado obtenido: %d\n", shared_counter);
    
    if (shared_counter != 20000) {
        printf("  [\033[32mPASS\033[0m] Condición de carrera detectada correctamente.\n");
        return 0;
    } else {
        printf("  [\033[31mFAIL\033[0m] No se detectó carrera (posiblemente la máquina es muy rápida).\n");
        return 1;
    }
}
