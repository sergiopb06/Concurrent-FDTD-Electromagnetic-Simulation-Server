#ifndef JOB_TABLE_H
#define JOB_TABLE_H
 
#include "job.h"
#include <stddef.h>

typedef enum{
    SUBMIT_OK, 
    SUBMIT_BUSY,
    SUBMIT_FULL,
    SUBMIT_STOPPING
} submit_result_t;


//QUEUE
int job_table_init(size_t queue_capacity);


void job_table_stop(void);


void job_table_destroy(void);


submit_result_t job_submit(const job_params_t *params, unsigned long *id);


//returns 0 if job is found, -1 if is not
int job_get(unsigned long id, job_t *out, unsigned long *position);


//Blocks until there is a job, marks it RUNNING. Returns -1 when server is stopped.
int job_next(job_t *out);


//Mark job DONE, store results
void job_finish(unsigned long id, double seconds, double sum_abs_ez, const char *file);

#endif //JOB_TABLE_H
