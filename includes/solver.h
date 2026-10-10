#ifndef SOLVER_H
#define SOLVER_H
#include "fdtd.h"

int fdtd_run(fdtdGrid *grid, int total_steps, int thread_count, int source_row, int source_column);


#endif 