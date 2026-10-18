#define _POSIX_C_SOURCE 200809L
 
#include "../includes/dispatcher.h"
#include "../includes/job_table.h"
#include "../includes/fdtd.h"      
#include "../includes/solver.h"    
#include "../includes/state.h"
#include "../includes/bqueue.h"
#include "../includes/snapshot.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
 
static pthread_t g_dispatcher;
static pthread_t g_writer;
static published_state_t g_state;
static bqueue_t g_snap_queue;
 
static double now_seconds(void){

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
 
static void *dispatcher_loop(void *arg){

    (void)arg;
    job_t job;
 
    //job_next blocks while the queue is empty, returns -1 on SHUTDOWN
    while (job_next(&job) == 0){
        double start = now_seconds();
        const job_params_t *params = &job.params;
 
        fdtdGrid grid;
        if(fdtd_grid_init(&grid, params->nx, params->ny) != 0){
            job_fail(job.id);
            continue;
        }

        published_state_start_job(&g_state, job.id, params->steps, params->nx, params->ny);
                                                                                                                                //if k > 0 -> &g_snap_queue  else -> NULL
        int status = fdtd_run_extended(&grid, params->steps, params->threads, params->sx, params->sy, params->k, job.id, &g_state, params->k > 0 ? &g_snap_queue : NULL);

        double sum_abs_ez = fdtd_sum_abs_ez(&grid);
        fdtd_grid_free(&grid);
        published_state_finish_job(&g_state);

        if(status != 0){
            job_fail(job.id);
            continue;
        }

        char file[256];
        if(params->k > 0)
            snprintf(file, sizeof file, "job%lu_step%d.bin", job.id, ((params->steps - 1) / params->k) * params->k);

        else    
            snprintf(file, sizeof file, "-"); //NO FILE EXISTS
        


        job_finish(job.id, now_seconds() - start, sum_abs_ez, file);
    }
 
    return NULL;
}
 
int dispatcher_start(void){
    if (published_state_init(&g_state) != 0)
        return -1;

    
    if (bqueue_init(&g_snap_queue, 4) != 0){
        published_state_destroy(&g_state);
        return -1;
    }

    if (pthread_create(&g_writer, NULL, snapshot_writer_thread, &g_snap_queue) != 0){
        bqueue_destroy(&g_snap_queue);
        published_state_destroy(&g_state);
        return -1;
    }

    
    if (pthread_create(&g_dispatcher, NULL, dispatcher_loop, NULL) != 0){
        bqueue_stop(&g_snap_queue);
        pthread_join(g_writer, NULL);
        bqueue_destroy(&g_snap_queue);
        published_state_destroy(&g_state);
        return -1;
    }


    return 0;
}

//SHUTDOWN in order
void dispatcher_stop(void){
    job_table_stop();
    pthread_join(g_dispatcher, NULL);
    bqueue_stop(&g_snap_queue);
    pthread_join(g_writer, NULL);
    bqueue_destroy(&g_snap_queue);
    published_state_destroy(&g_state);
}


int dispatcher_get_status(unsigned long id, int *step, int *total){
    return published_state_get_status(&g_state, id, step, total);
}

int dispatcher_peek(unsigned long id, int i, int j, int *step, double *val){
    return published_state_get_peek(&g_state, id, i, j, step, val);
}
 
