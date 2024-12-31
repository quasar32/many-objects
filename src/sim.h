#pragma once

#define N_BALLS 4096
#define RADIUS 0.4f
#define DIAMETER 0.8f
#define SPS 600
#define DT (1.0f / SPS)
#define N_STEPS (10 * SPS)

#include <cglm/struct.h>

extern vec3s sim_x[N_BALLS];
extern vec3s sim_x0[N_BALLS];
extern vec3s sim_v[N_BALLS];

void init_sim(void);
void step_sim(void);
