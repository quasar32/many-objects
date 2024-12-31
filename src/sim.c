#include "sim.h"
#include <math.h>
#include <string.h>
#include <pthread.h>

#define GRID_LEN 32
#define GRID_MIN (RADIUS - GRID_LEN / 2)
#define GRID_MAX (GRID_LEN / 2 - RADIUS)

vec3s sim_x[N_BALLS];
vec3s sim_x0[N_BALLS];
vec3s sim_v[N_BALLS];

static uint16_t nodes[N_BALLS];
static uint16_t grid[GRID_LEN][GRID_LEN][GRID_LEN];

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

static void init_positions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim_x[i].x = i % 16 - GRID_LEN / 4 + 0.5f;
        sim_x[i].y = i / 16 % 16 - GRID_LEN / 4 + 0.5f;
        sim_x[i].z = i / 256 - GRID_LEN / 4 + 0.5f;
    }
}

static void init_velocities(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim_v[i].x = 2.0 * drand48() - 1.0;
        sim_v[i].y = 2.0 * drand48() - 1.0;
        sim_v[i].z = 2.0 * drand48() - 1.0;
    }
}

void init_sim(void) {
    init_positions();
    init_velocities();
}

static float fclampf(float v, float l, float h) {
    return fminf(fmaxf(v, l), h);
}

static void symplectic_euler(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim_v[i].y -= 10.0f * DT;
        sim_x0[i] = sim_x[i];
        sim_x[i] = vec3_muladds(sim_v[i], DT, sim_x[i]);
        sim_x[i].x = fclampf(sim_x[i].x, GRID_MIN, GRID_MAX);
        sim_x[i].y = fclampf(sim_x[i].y, GRID_MIN, GRID_MAX);
        sim_x[i].z = fclampf(sim_x[i].z, GRID_MIN, GRID_MAX);
        sim_v[i] = vec3_sub(sim_x[i], sim_x0[i]);
        sim_v[i] = vec3_scale(sim_v[i], SPS);
    }
}

static void init_grid(void) {
    memset(grid, 0xFF, sizeof(grid));
    for (int i = 0; i < N_BALLS; i++) {
        int x = sim_x[i].x + GRID_LEN / 2;
        int y = sim_x[i].y + GRID_LEN / 2;
        int z = sim_x[i].z + GRID_LEN / 2;
        nodes[i] = grid[x][y][z];
        grid[x][y][z] = i;
    }
}

static void resolve_collisions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        int bx = sim_x[i].x + GRID_LEN / 2;
        int by = sim_x[i].y + GRID_LEN / 2;
        int bz = sim_x[i].z + GRID_LEN / 2;
        int x0 = max(bx - 1, 0); 
        int y0 = max(by - 1, 0);
        int z0 = max(bz - 1, 0);
        int x1 = min(bx + 1, GRID_LEN - 1); 
        int y1 = min(by + 1, GRID_LEN - 1);
        int z1 = min(bz + 1, GRID_LEN - 1);
        for (int x = x0; x <= x1; x++) {
            for (int y = y0; y <= y1; y++) {
                for (int z = z0; z <= z1; z++) {
                    int j = grid[x][y][z]; 
                    for (; j != UINT16_MAX; j = nodes[j]) {
                        if (j < i) {
                            vec3s normal = vec3_sub(sim_x[i], sim_x[j]);
                            float d2 = vec3_norm2(normal); 
                            if (d2 > 0.0f && d2 < DIAMETER * DIAMETER) {
                                float d = sqrtf(d2);
                                normal = vec3_divs(normal, d);
                                float corr = (DIAMETER - d) / 2.0f;
                                vec3s dx = vec3_scale(normal, corr);
                                sim_x[i] = vec3_add(sim_x[i], dx);
                                sim_x[j] = vec3_sub(sim_x[j], dx);
                                float vi = vec3_dot(sim_v[i], normal);
                                float vj = vec3_dot(sim_v[j], normal);
                                vec3s dv = vec3_scale(normal, vi - vj);
                                sim_v[i] = vec3_sub(sim_v[i], dv);
                                sim_v[j] = vec3_add(sim_v[j], dv);
                            }
                        }
                    }
                }
            }
        }
    }
}

void step_sim(void) {
    symplectic_euler();
    init_grid();
    resolve_collisions();
}
