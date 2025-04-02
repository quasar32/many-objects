#include "sim.h"
#include "worker.h"
#include <math.h>
#include <string.h>

void init_positions(void);
void init_velocities(void);
void init_grid(void);

void init_sim(int argc, char **argv) {
    init_positions();
    init_velocities();
}

static float fclampf(float v, float l, float h) {
    return fminf(fmaxf(v, l), h);
}

static void symplectic_euler(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.v[i].y -= 10.0f * DT;
        sim.x0[i] = sim.x[i];
        sim.x[i] = vec4_muladds(sim.v[i], DT, sim.x[i]);
        sim.x[i].x = fclampf(sim.x[i].x, GRID_MIN, GRID_MAX);
        sim.x[i].y = fclampf(sim.x[i].y, GRID_MIN, GRID_MAX);
        sim.x[i].z = fclampf(sim.x[i].z, GRID_MIN, GRID_MAX);
    }
}

static void newton_rasphon(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.v[i] = vec4_sub(sim.x[i], sim.x0[i]);
        sim.v[i] = vec4_scale(sim.v[i], SPS);
    }
}

static void resolve_ball_ball_collision(int i, int j) {
    vec4s normal = vec4_sub(sim.x[i], sim.x[j]);
    float d2 = vec4_norm2(normal);
    if (d2 > 0.0f && d2 < DIAMETER * DIAMETER) {
        float d = sqrtf(d2);
        normal = vec4_divs(normal, d);
        float corr = (DIAMETER - d) / 2.0f;
        vec4s dx = vec4_scale(normal, corr);
        sim.x[i] = vec4_add(sim.x[i], dx);
        sim.x[j] = vec4_sub(sim.x[j], dx);
    }
}

static inline int min(int a, int b) {
    return a < b ? a : b;
}

static inline int max(int a, int b) {
    return a > b ? a : b;
}

void step_sim(void) {
    symplectic_euler();
    newton_rasphon();
    init_grid();
    resolve_collisions();
}

void print_profile(void) {}
void resolve_pair_collisions(void) {}
