#include <stdio.h>
#include "../includes/fdtd.h"
#include "../includes/solver.h"
#include <math.h>

#define REF_N 8     /*Variables used for reference test*/
#define REF_STEPS 5 
#define REF_SOURCE 4 

static int fails = 0;
# The following block of code was partially made using: 
# Anthropic (2026). Claude(Opus 5.5) https://claude.ai
# */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define CHECK(condition, message)                                   \
    do {                                                            \
        if (condition) {                                            \
            printf("PASS: %s\n", message);                          \
        } else {                                                    \
            printf("FAIL: %s (line %d)\n", message, __LINE__);      \
            exit(1);                                                \
        }                                                           \
    } while (0)

#include "../includes/rwlock.h"

static int shared_counter = 0;
static rwlock_t lock;

static void *safe_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 10000; i++) {
        rwlock_wrlock(&lock);
        shared_counter++;
        rwlock_wrunlock(&lock);
    }
    return NULL;
}

static void test_fixed_race(void) {
    rwlock_init(&lock);
    pthread_t th1, th2;
    pthread_create(&th1, NULL, safe_worker, NULL);
    pthread_create(&th2, NULL, safe_worker, NULL);
    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    
    printf("Resultado con sincronización: %d\n", shared_counter);
    CHECK(shared_counter == 20000, "La sincronización elimina la condición de carrera");
    rwlock_destroy(&lock);
}

int main(void) {
    test_fixed_race();
    return 0;
}



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

static void reference_8x8(double ez[REF_N][REF_N]){

    const double sc = 1.0 / sqrt(2.0);
    const double eta0 = 376.730313668;
    const double eps_r = 1.0;
    const double ppw = 20.0;
    const double pi = 3.14159265358979323846;
    double hx[REF_N][REF_N - 1] = {{0.0}};
    double hy[REF_N - 1][REF_N] = {{0.0}};

    for(int m = 0; m < REF_N; m++){
        for(int n = 0; n < REF_N; n++){
            ez[m][n] = 0.0;
        }
    }

    /*Magnetic field */
    for(int q = 0; q < REF_STEPS; q++){

        for(int m = 0; m < REF_N; m++){
            for(int n = 0; n < REF_N - 1; n++){
                hx[m][n] = hx[m][n] - (sc/eta0) * (ez[m][n+1] - ez[m][n]);
            }
        }

        for(int m = 0; m < REF_N - 1; m++){
            for(int n = 0; n < REF_N; n++){
                hy[m][n] = hy[m][n] + (sc/eta0) * (ez[m+1][n] - ez[m][n]);
            }
        }

        /*Electric field*/
        for(int m = 1; m < REF_N - 1; m++){
            for(int n = 1; n < REF_N - 1; n++){
                ez[m][n] = ez[m][n] + (sc * eta0 / eps_r) * ((hy[m][n] - hy[m-1][n]) - (hx[m][n] - hx[m][n-1]));
            }
        }

        /*Ricker source*/
        double arg = pi * ((sc*q) / ppw - 1.0);
        arg = arg * arg; 
        ez[REF_SOURCE][REF_SOURCE] += (1.0 - 2.0 * arg) * exp(-arg);
    }

}

static void test_reference_8x8(void){
    double expected[REF_N][REF_N];
    fdtdGrid grid;
    int bad_cells = 0;
    double max_difference = 0.0;

    reference_8x8(expected); 

    fdtd_grid_init(&grid, REF_N, REF_N);
    fdtd_run(&grid, REF_STEPS, 1, REF_SOURCE, REF_SOURCE);

    for(int m = 0; m < REF_N; m++){
        for(int n = 0; n <REF_N;n++ ){
            double difference = fabs(grid.Ez[m * grid.ny + n] - expected[m][n]);
            if(difference  > max_difference){
                max_difference = difference;
            }
            if(difference > 1e-12){
                bad_cells++;
            }
        }
    }

    printf(" max difference: %.3e\n", max_difference);
    CHECK(bad_cells == 0, "Reference 8x8 with 5 steps: matches independent implementation");
    fdtd_grid_free(&grid);

}

int main(){
    test_borders_zero();
    test_symmetry();
    test_reference_8x8();
    if(fails == 0){
        printf("\nAll tests passed.");
        return 0;
    }
    printf("\n%d test(s) failed. \n", fails);
    
}