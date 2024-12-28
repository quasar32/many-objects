#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "sim.h"

#define IS_LONG(v) _Generic((v), long: 1, default: 0)

static long get_time(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t) < 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    static_assert(IS_LONG(t.tv_sec));
    static_assert(IS_LONG(t.tv_nsec));
    return t.tv_sec * 1000000000 + t.tv_nsec;
}

int main(int argc, char **argv) {
    init_sim();
    long t0 = get_time();
    for (int t = 0; t < N_FRAMES; t++) {
        step_sim();
    }
    long t1 = get_time();
    long dt = (t1 - t0) / 1000000; 
    fprintf(stderr, "%ld ms\n", dt); 
    return 0;
}
