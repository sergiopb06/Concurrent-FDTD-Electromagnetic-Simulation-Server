#define _POSIX_C_SOURCE 200809L
 
#include "../includes/commands.h"
#include "../includes/job_table.h"
#include <stdio.h>


void command_execute(const command_t *command, char *reply, size_t size){

    job_t job;
    unsigned long position = 0;

    switch (command->type)
    {

    //SUBMIT
    case C_SUBMIT: {
        unsigned long id = 0;

        switch (job_submit(&command->params, &id))
        {
        case SUBMIT_OK:
            snprintf(reply, size, "OK %lu", id);
            break;
        
        case SUBMIT_BUSY:
            snprintf(reply, size, "BUSY");
            break;

        case SUBMIT_STOPPING:
            snprintf(reply, size, "ERROR server shutting down");
            break;

        default:
            snprintf(reply, size, "ERROR job table full");
            break;
        }

        break;
    }

    //STATUS
    case C_STATUS:
        if(job_get(command->id, &job, &position) < 0){
            snprintf(reply, size, "ERROR unknown job");
            break;
        }

        switch (job.status)
        {
        case JOB_QUEUED:
            snprintf(reply, size, "QUEUED %lu", position);
            break;
        
        case JOB_RUNNING:
            snprintf(reply, size, "RUNNING 0/%d", job.params.steps);
            break;
        
        case JOB_DONE:
            snprintf(reply, size, "DONE %.3f", job.seconds);
            break;

        default:
            snprintf(reply, size, "ERROR job failed");
            break;
        }

        break;

    //PEEK
    case C_PEEK:
        if (job_get(command->id, &job, &position) < 0)
            snprintf(reply, size, "ERROR unknown job");

        else if (command->i >= job.params.nx || command->j >= job.params.ny)
            snprintf(reply, size, "ERROR cell out of grid");

        else
            snprintf(reply, size, "ERROR no snapshot yet"); //NEEDS SNAPSHOTS FROM SOLVER

        break;


    //RESULT
    case C_RESULT:
        if(job_get(command->id, &job, &position) < 0)
            snprintf(reply, size, "ERROR unknown job");

        else if (job.status != JOB_DONE)
            snprintf(reply, size, "ERROR job not finished");

        else    
            snprintf(reply, size, "RESULT %.10e %s", job.sum_abs_ez, job.file); //NEEDS FILE

        break;


    //SHUTDOWN
    case C_SHUTDOWN:
        snprintf(reply, size, "ERROR not implemented"); //needs the orderly shutdown
        break;

    
    default:
        snprintf(reply, size, "ERROR");
        break;
    }
}