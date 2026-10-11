#include <stdio.h>
#include "../includes/fdtd.h"
#include "../includes/solver.h"

static int fails = 0;

/* 
The following macro was partially made using: 
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

int main(){
    test_borders_zero();
    if(fails == 0){
        printf("\nAll tests passed.");
        return 0;
    }
    printf("\n%d test(s) failed. \n", fails);
    
}