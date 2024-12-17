#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "sim.h"

#define N_FRAMES (20 * FPS)

static int ends_only;

static void print_sim(int t) {
    for (int i = 0; i < N_BALLS; i++) {
        printf("%d,%f,%f,%f\n", t, balls_pos[i][0], 
                balls_pos[i][1], balls_pos[i][2]);
    }
}

static void parse_args(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "e")) != -1) {
    switch (c) {
    case 'e':
      ends_only = 1;
      break;
    default:
      exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
    if (ends_only) {
        printf("t,x,y,z\n");
        init_sim();
        print_sim(0);
        clock_t begin = clock();
        for (int t = 0; t < N_FRAMES; t++) 
            update_sim();
        clock_t end = clock();
        print_sim(N_FRAMES);
        int diff = (end - begin) * 1000 / CLOCKS_PER_SEC;
        fprintf(stderr, "%d ms\n", diff); 
    } else {
        printf("t,x,y,z\n");
        init_sim();
        for (int t = 0; t < N_FRAMES; t++) {
            print_sim(t);
            update_sim();
        }
        print_sim(N_FRAMES);
    }
    return 0;
}
