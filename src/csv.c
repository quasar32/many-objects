#include <stdio.h>
#include <stdlib.h>
#include "sim.h"

static void print_sim(int t) {
    for (int i = 0; i < N_BALLS; i++) {
        printf("%d,%f,%f,%f\n", t, balls_pos[i][0], 
                balls_pos[i][1], balls_pos[i][2]);
    }
}

int main(void) {
    printf("t,x,y,z\n");
    init_sim();
    int t;
    for (t = 0; t < 10 * FPS; t++) {
        print_sim(t);
        update_sim();
    }
    print_sim(t);
    return 0;
}
