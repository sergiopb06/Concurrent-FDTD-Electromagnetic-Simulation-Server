#ifndef FDTD_H
#define FDTD_H

/*Define variables and constants used for calculations and formulas*/

#define FDTD_SC 0.70710678118654752440
#define FDTD_N0 376.730313668
#define FDTD_ER  1.0
#define FDTD_PPW 20.0
#define FDTD_PI 3.14159265358979323846

typedef struct {
    int nx;  // Number of cells in x = rows in Ez 
    int ny; // Number of cells in y = columns in Ez
    double *Ez; 
    double *Hx;
    double *Hy; 
}fdtdGrid; 

int fdtd_grid_init(fdtdGrid *grid, int nx, int ny);

void fdtd_grid_free(fdtdGrid *grid);

double fdtd_ricker(long step);

void fdtd_update_h(fdtdGrid *grid, int row_start, int row_end);

void fdtd_update_e(fdtdGrid *grid, int row_start, int row_end);

void fdtd_add_source(fdtdGrid *g, int row_source, int column_source, long step);

double fdtd_sum_abs_ez(const fdtdGrid *grid);

#endif