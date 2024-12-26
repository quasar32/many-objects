#pragma once

#define N_BALLS 32768 
#define RADIUS 0.4f
#define DIAMETER 0.8f
#define FPS 60
#define DT (1.0f / FPS)
#define N_FRAMES (30 * FPS)

#include <cglm/cglm.h>

extern vec3 balls_pos[N_BALLS];
extern vec3 balls_pos0[N_BALLS];
extern vec3 balls_vel[N_BALLS];

void init_sim(void);
void step_sim(void);
