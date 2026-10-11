#ifndef JOB_H
#define JOB_H

//SUBMIT parameters
typedef struct {
    int nx, ny; //grid size
    int steps;
    int threads; 
    int k; //k step
    int sx, sy; //source, columns and rows
} job_params_t;

//STATUS
typedef enum {
    JOB_QUEUED,
    JOB_RUNNING,
    JOB_DONE, 
    JOB_ERROR
} job_status_t;


//JOB
typedef struct {

    unsigned long id;
    job_params_t params;
    //JOB DONE
    job_status_t status;
    double seconds; 
    double sum_abs_ez;
    char file[256]; 
} job_t;

#endif//JOB_H