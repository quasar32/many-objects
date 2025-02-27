#pragma once

#define CL_TARGET_OPENCL_VERSION 300
#define N_BALLS 4096
#define RADIUS 0.4f
#define DIAMETER 0.8f
#define SPS 600
#define DT (1.0f / SPS)
#define N_STEPS (10 * SPS)
#define GRID_LEN 32
#define GRID_MIN (RADIUS - GRID_LEN / 2)
#define GRID_MAX (GRID_LEN / 2 - RADIUS)

#include <cglm/struct.h>

struct sim {
    vec4s x[N_BALLS];
    vec4s x0[N_BALLS];
    vec4s v[N_BALLS];
    short nodes[N_BALLS];
    short grid[GRID_LEN][GRID_LEN][GRID_LEN];
    short tx, ty, tz;
    short px, py, pz;
    short nx, ny, nz;
    short ix, iy, iz;
};

extern struct sim sim;

void init_sim(void);
void step_sim(void);
