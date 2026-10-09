#include<pthread.h>
#include "fdtd.h"


//#include "barrier.h"

typedef struct{
    int thread_id;
    int row_start;
    int row_end;
    fdtdGrid *grid;
    int total_steps;
    int source_row;
    int source_column;
    //barrier_t *barrier; 
}solver_thread_args_t;

static void *solver_thread(void *arg){
    solver_thread_args_t *args = (solver_thread_args_t *)arg;

    for(int i = 0; i < args->total_steps; i++ ){
        fdtd_update_h(args->grid, args->row_start, args->row_end);
        //barrier 
        fdtd_update_e(args->grid, args->row_start, args->row_end);
        
        if(args->source_row >= args->row_start && args->source_row < args->row_end){
            fdtd_add_source(args->grid, args->source_row, args->source_column, i);
        }
        //barrier
    }

    return NULL;

}

int fdtd_run(fdtdGrid *grid, int total_steps, int thread_count, int source_row, int source_column){
    pthread_t threads[thread_count];
    solver_thread_args_t argsArray[thread_count];

    //Initalize barrier

    int quotient = grid->nx / thread_count;
    int remainder = grid->nx % thread_count;    

    for(int i = 0; i < thread_count; i++){

        int extra_rows = i;
        if(remainder < i){
            extra_rows = remainder; 
        }

        int start = i * quotient + extra_rows; 
        int end = start + quotient;  // If i < remainder, end++ 

        if(i < remainder){
            end++; 
        }

        argsArray[i].row_start = start; 
        argsArray[i].row_end = end;
        argsArray[i].thread_id = i;
        argsArray[i].grid = grid;
        argsArray[i].total_steps = total_steps; 
        argsArray[i].source_column = source_column;
        argsArray[i].source_row = source_row;



        pthread_create(&threads[i], NULL, solver_thread, &argsArray[i]);

    }

    for(int i = 0; i < thread_count; i++){
        pthread_join(threads[i], NULL);
    }

    //Destroy barrier
    return 0;
}



