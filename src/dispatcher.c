#define _POSIX_C_SOURCE 200809L
 
#include "../includes/dispatcher.h"
#include "../includes/job_table.h"
#include "../includes/fdtd.h"      
#include "../includes/solver.h"    
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
 
static pthread_t g_dispatcher;
 
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
        double sum_abs_ez = 0.0;
 
        //HERE GOES SOLVER
 
        sleep(2); //REMOVE WHEN SOLVER IMPLEMENTED

        job_finish(job.id, now_seconds() - start, sum_abs_ez, "FILE"); //NEED TO ADD FILE 
    }
 
    return NULL;
}
 
int dispatcher_start(void){
    return pthread_create(&g_dispatcher, NULL, dispatcher_loop, NULL) == 0 ? 0 : -1;
}
 
void dispatcher_stop(void){
    job_table_stop();
    pthread_join(g_dispatcher, NULL);
}
 
