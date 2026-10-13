#define _POSIX_C_SOURCE 200809L
 
#include "../includes/job_table.h"
#include "../includes/bqueue.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#define MAX_JOBS 1024

static job_t g_jobs[MAX_JOBS];
static unsigned long g_job_count = 0;
static pthread_mutex_t g_jobs_mutex = PTHREAD_MUTEX_INITIALIZER;

static bqueue_t g_queue; //holds jobs ids as pointers

static int g_stopping = 0;

int job_table_init(size_t queue_capacity){
    return bqueue_init(&g_queue, queue_capacity);
}

void job_table_stop(void){
    pthread_mutex_lock(&g_jobs_mutex);
    g_stopping = 1;
    pthread_mutex_unlock(&g_jobs_mutex);

    bqueue_stop(&g_queue);//wakes dispatcher
}

void job_table_destroy(void){
    bqueue_destroy(&g_queue);
}


void job_fail(unsigned long id){
    pthread_mutex_lock(&g_jobs_mutex);
    g_jobs[id - 1].status = JOB_ERROR;
    pthread_mutex_unlock(&g_jobs_mutex);
}

submit_result_t job_submit(const job_params_t *params, unsigned long *id){

    submit_result_t result;

    pthread_mutex_lock(&g_jobs_mutex);

    if(g_job_count == MAX_JOBS){
        result = SUBMIT_FULL;
    }

    else{

        job_t *job = &g_jobs[g_job_count];
        memset(job, 0, sizeof *job);
        job->id = g_job_count + 1;
        job->params = *params;
        job->status = JOB_QUEUED;


        if(bqueue_try_push(&g_queue, (void *)(uintptr_t)job->id) == 0 ){
            g_job_count++;
            *id = job->id;
            result = SUBMIT_OK;
        }

        else{    //if errno == ENCANCELED -> result = SUBMIT_STOPPING | else = SUBMIT_BUSY
            result = (errno == ECANCELED) ? SUBMIT_STOPPING : SUBMIT_BUSY;
        }
    }

    pthread_mutex_unlock(&g_jobs_mutex);
    return result;
}



int job_get(unsigned long id, job_t *out, unsigned long *position){

    int found = -1;

    pthread_mutex_lock(&g_jobs_mutex);

    if(id >= 1 && id <= g_job_count){
        *out = g_jobs[id - 1]; //copy of job

        *position = 1;
        for(unsigned long n = 0; n < id - 1; n++)
            if(g_jobs[n].status == JOB_QUEUED)
                (*position)++;

        found = 0;
    }

    pthread_mutex_unlock(&g_jobs_mutex);
    return found;
}


int job_next(job_t *out){

    void *item = NULL;

    //Barber sleeps
    if(bqueue_pop(&g_queue, &item) < 0)
        return -1;

    unsigned long id = (unsigned long)(uintptr_t)item;

    pthread_mutex_lock(&g_jobs_mutex);

    //SHUTDOWN, current job is finished, queued ones are not started.
    if(g_stopping){
        pthread_mutex_unlock(&g_jobs_mutex);
        return -1;
    }

    g_jobs[id - 1].status = JOB_RUNNING;
    *out = g_jobs[id - 1];

    pthread_mutex_unlock(&g_jobs_mutex);
    return 0;
}


void job_finish(unsigned long id, double seconds, double sum_abs_ez, const char *file){

    pthread_mutex_lock(&g_jobs_mutex);
 
    job_t *job = &g_jobs[id - 1];
    job->seconds = seconds;
    job->sum_abs_ez = sum_abs_ez;
    
    snprintf(job->file, sizeof job->file, "%s", file);

    job->status = JOB_DONE;
 
    pthread_mutex_unlock(&g_jobs_mutex);
}
 
