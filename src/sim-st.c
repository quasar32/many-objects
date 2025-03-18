#include "sim.h"
#include "worker.h"
#include <math.h>
#include <string.h>

void init_positions(void);
void init_velocities(void);
void init_grid(void);

void init_sim(void) {
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
        float vi = vec4_dot(sim.v[i], normal);
        float vj = vec4_dot(sim.v[j], normal);
        vec4s dv = vec4_scale(normal, vi - vj);
        sim.v[i] = vec4_sub(sim.v[i], dv);
        sim.v[j] = vec4_add(sim.v[j], dv);
    }
}

static inline int min(int a, int b) {
    return a < b ? a : b;
}

static inline int max(int a, int b) {
    return a > b ? a : b;
}

static void resolve_collisions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        int x = sim.x[i].x + GRID_LEN / 2;
        int y = sim.x[i].y + GRID_LEN / 2;
        int z = sim.x[i].z + GRID_LEN / 2;
        int x0 = max(x - 1, 0);
        int y0 = max(y - 1, 0);
        int z0 = max(z - 1, 0);
        int x1 = min(x + 2, GRID_LEN);
        int y1 = min(y + 2, GRID_LEN);
        int z1 = min(z + 2, GRID_LEN);
        for (x = x0; x < x1; x++) {
            for (y = y0; y < y1; y++) {
                for (z = z0; z < z1; z++) {
                    for (int j = sim.grid[x][y][z]; j >= 0; j = sim.nodes[j]) {
                        resolve_ball_ball_collision(i, j);
                    }
                }
            }
        }
    }
}

void step_sim(void) {
    activate_workers();
    symplectic_euler();
    init_grid();
    resolve_collisions();
    deactivate_workers();
}

void print_profile(void) {}
void resolve_pair_collisions(void) {}
