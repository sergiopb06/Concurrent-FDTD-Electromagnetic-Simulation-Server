#include <stdio.h>
#include "../includes/fdtd.h"
#include "../includes/solver.h"

static int fails = 0;

/* 
The following block of code was partially made using: 
Anthropic (2026). Claude(Opus 5.5) https://claude.ai
*/


#define CHECK(condition, message)                                   \
    do {                                                            \
        if (condition) {                                            \
            printf("PASS: %s\n", message);                          \
        } else {                                                    \
            printf("FAIL: %s (line %d)\n", message, __LINE__);      \
            fails++;                                             \
        }                                                           \
    } while (0)



static void test_borders_zero(void){
    fdtdGrid grid;
    int bad_cells = 0;

    fdtd_grid_init(&grid, 33, 33);
    fdtd_run(&grid, 60, 1, 16, 16);

    for (int m = 0; m < grid.nx; m++) {
        for (int n = 0; n < grid.ny; n++) {
            int on_border = (m == 0 || m == grid.nx - 1 || n == 0 || n == grid.ny - 1);
            if (on_border && grid.Ez[m * grid.ny + n] != 0.0) {
                bad_cells++;
            }
        }
    }

    CHECK(bad_cells == 0, "PEC borders: Ez is zero on the whole boundary");
    fdtd_grid_free(&grid);
}

static void test_symmetry(void){

    fdtdGrid grid;
    int bad_axis_m = 0;
    int bad_axis_n = 0;
    int bad_diagonal = 0; 
    int bad_anti_diagonal = 0; 

    fdtd_grid_init(&grid, 41, 41);
    fdtd_run(&grid, 80, 1, 20, 20);
    
    int last = grid.nx - 1;
    for(int m = 0; m < grid.nx; m++){

        for(int n = 0; n < grid.ny; n++){
        
            double value = grid.Ez[m * grid.ny + n];

            if(value != grid.Ez[(last - m) * grid.ny + n]){
                bad_axis_m++;
            }

            if(value != grid.Ez[m * grid.ny + (last - n)]){
                bad_axis_n++;
            }

            if(value != grid.Ez[n * grid.ny + m]){
                bad_diagonal++;
            }

            if (value != grid.Ez[(last - n) * grid.ny + (last -m)]){
                bad_anti_diagonal++;
            }
        }
    }

    CHECK(bad_axis_m == 0, "Symmery of orizontal axis");
    CHECK(bad_axis_n == 0, "Symmetry of vertical axis");
    CHECK(bad_diagonal == 0, "Symmetry of the main diagonal");
    CHECK(bad_anti_diagonal == 0, "Symmetry of thee anti-diagonal");
    fdtd_grid_free(&grid);
}


int main(){
    test_borders_zero();
    test_symmetry();
    if(fails == 0){
        printf("\nAll tests passed.");
        return 0;
    }
    printf("\n%d test(s) failed. \n", fails);
    
}