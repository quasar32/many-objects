#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "sim.h"

int main(int argc, char **argv) {
    init_sim();
    clock_t begin = clock();
    for (int t = 0; t < N_FRAMES; t++) {
        step_sim();
    }
    clock_t end = clock();
    clock_t ms = (end - begin) / 1000;
    fprintf(stderr, "%ld ms\n", ms); 
    return 0;
}
