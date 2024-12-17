#pragma once

#include <cglm/cglm.h>

#define N_BALLS 4096
#define RADIUS 0.4f
#define DIAMETER 0.8f
#define FPS 60
#define DT (1.0f / FPS)

extern vec3 balls_pos[N_BALLS];
extern vec3 balls_pos0[N_BALLS];
extern vec3 balls_vel[N_BALLS];
