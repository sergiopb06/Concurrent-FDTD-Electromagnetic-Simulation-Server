#include <stdlib.h>
#include "fdtd.h"
#include <math.h>


int fdtd_grid_init(fdtdGrid *grid, int nx, int ny){
    size_t n_Ez, n_Hx, n_Hy;

    if (grid == NULL || nx < 3 || ny < 3){
        return -1; 
    }

    n_Ez = (size_t)nx * (size_t)ny;
    n_Hx = (size_t)nx * (size_t)(ny - 1);
    n_Hy = (size_t)(nx - 1) * (size_t)ny;

    grid->nx = nx; 
    grid->ny = ny;
    grid->Ez = calloc(n_Ez, sizeof(double));
    grid->Hx = calloc(n_Hx, sizeof(double));
    grid->Hy = calloc(n_Hy, sizeof(double));

    if(grid->Ez == NULL || grid->Hx == NULL || grid->Hy == NULL){
        fdtd_grid_free(grid);
        return -1;
    }

    return 0;
}

void fdtd_grid_free(fdtdGrid *g){
    if(g == NULL){
        return;
    }

    free(g->Ez);
    free(g->Hx);
    free(g->Hy);
    g->Ez = NULL;
    g->Hx = NULL;
    g->Hy = NULL;
    g->nx = 0;
    g->ny = 0;
}

double fdtd_ricker(long step){
    double time_relative = (FDTD_SC * (double)step) / FDTD_PPW - 1.0;
    double arg = (FDTD_PI * time_relative) * (FDTD_PI * time_relative);
    
    return (1.0 - 2.0 * arg) * exp(-arg);
}

void fdtd_update_h(fdtdGrid *g, int  row_start, int row_end){
    
    const int nx = g->nx;
    const int ny = g->ny;
    const double coefficient_h = FDTD_SC / FDTD_N0; 
    int m, n;

    if(row_start < 0){
        row_start = 0;
    }

    if(row_end > nx){
        row_end = nx;
    }
    for(m = row_start; m < row_end; m++){
        for(n = 0; n < ny - 1; n++){
            g->Hx[m * (ny - 1) + n] -= coefficient_h * (g->Ez[m * ny + n + 1] - g->Ez[m * ny + n]);
        }
    }
    if(row_end > nx - 1){
        row_end = nx - 1; 
    }
    for(m = row_start; m < row_end; m++){
        for(n = 0; n < ny; n++){
            g->Hy[m * ny + n] += coefficient_h * (g->Ez[(m+1) * ny + n] - g->Ez[m * ny + n]);
        }
    }
}

void fdtd_update_e(fdtdGrid *g, int row_start, int row_end){
    const int nx = g->nx;
    const int ny = g->ny; 
    const double coefficient_e = FDTD_SC * FDTD_N0 / FDTD_ER;
    int m, n; 

    if(row_start < 1){
        row_start = 1;
    }
    if(row_end > nx -1){
        row_end = nx -1;
    }
    for(m = row_start; m < row_end; m++){
        for(n = 1; n < ny - 1; n++){
            double dhy = g->Hy[m* ny + n] - g->Hy[(m-1) * ny + n];
            double dhx = g->Hx[m*(ny - 1 ) + n] - g->Hx[m * (ny-1) + n - 1];
            g->Ez[m * ny + n] += coefficient_e * (dhy - dhx);
        }
    }
}

void fdtd_add_source(fdtdGrid *g,int row_source, int column_source, long step){
    g->Ez[row_source * g->ny + column_source] += fdtd_ricker(step);
}

double fdtd_sum_abs_ez(const fdtdGrid *g){
    size_t i;
    size_t total_cells = (size_t)g->nx * (size_t)g->ny; 
    double sum = 0.0;

    for(i = 0; i < total_cells; i++){
        sum += fabs(g->Ez[i]);
    }
    return sum;
}