#include "sim.h"
#include "worker.h"
#include "misc.h"
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

struct sim sim;

static void init_positions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.x[i].x = i % 16 - GRID_LEN / 4 + 0.5f;
        sim.x[i].y = i / 16 % 16 - GRID_LEN / 4 + 0.5f;
        sim.x[i].z = i / 256 - GRID_LEN / 4 + 0.5f;
    }
}

static void init_velocities(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.v[i].x = 2.0 * drand48() - 1.0;
        sim.v[i].y = 2.0 * drand48() - 1.0;
        sim.v[i].z = 2.0 * drand48() - 1.0;
    }
}

void init_sim(void) {
    create_workers();
    init_positions();
    init_velocities();
}

static float fclampf(float v, float l, float h) {
    return fminf(fmaxf(v, l), h);
}

static void symplectic_euler(int worker_idx) {
    int i = worker_idx * N_BALLS / N_WORKERS;
    int n = (worker_idx + 1) * N_BALLS / N_WORKERS;
    for (; i < n; i++) {
        sim.v[i].y -= 10.0f * DT;
        sim.x0[i] = sim.x[i];
        sim.x[i] = vec3_muladds(sim.v[i], DT, sim.x[i]);
        sim.x[i].x = fclampf(sim.x[i].x, GRID_MIN, GRID_MAX);
        sim.x[i].y = fclampf(sim.x[i].y, GRID_MIN, GRID_MAX);
        sim.x[i].z = fclampf(sim.x[i].z, GRID_MIN, GRID_MAX);
        sim.v[i] = vec3_sub(sim.x[i], sim.x0[i]);
        sim.v[i] = vec3_scale(sim.v[i], SPS);
    }
}

static short *hash(unsigned x, unsigned y, unsigned z) {
    return sim.grid + ((x * 92837111) ^ (y * 689287499) ^ (z * 283923481)) % (N_BALLS * 2); 
}

static void init_grid(void) {
    memset(sim.grid, 0xFF, sizeof(sim.grid));
    for (int i = 0; i < N_BALLS; i++) {
        int x = floorf(sim.x[i].x);
        int y = floorf(sim.x[i].y);
        int z = floorf(sim.x[i].z);
        short *head = hash(x, y, z);
        sim.nodes[i] = *head;
        *head = i;
    }
}

static void resolve_ball_ball_collision(int i, int j) {
    vec3s normal = vec3_sub(sim.x[i], sim.x[j]);
    float d2 = vec3_norm2(normal);
    if (d2 > 0.0f && d2 < DIAMETER * DIAMETER) {
        float d = sqrtf(d2);
        normal = vec3_divs(normal, d);
        float corr = (DIAMETER - d) / 2.0f;
        vec3s dx = vec3_scale(normal, corr);
        sim.x[i] = vec3_add(sim.x[i], dx);
        sim.x[j] = vec3_sub(sim.x[j], dx);
        float vi = vec3_dot(sim.v[i], normal);
        float vj = vec3_dot(sim.v[j], normal);
        vec3s dv = vec3_scale(normal, vi - vj);
        sim.v[i] = vec3_sub(sim.v[i], dv);
        sim.v[j] = vec3_add(sim.v[j], dv);
    }
}

static void resolve_collisions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        int x = floorf(sim.x[i].x);
        int y = floorf(sim.x[i].y);
        int z = floorf(sim.x[i].z);
        int x0 = x - 1; 
        int y0 = y - 1;
        int z0 = z - 1;
        int x1 = x + 2;
        int y1 = y + 2;
        int z1 = z + 2;
        for (x = x0; x < x1; x++) {
            for (y = y0; y < y1; y++) {
                for (z = z0; z < z1; z++) {
                    for (int j = *hash(x, y, z); j >= 0; j = sim.nodes[j]) {
                        resolve_ball_ball_collision(i, j);
                    }
                }
            }
        }
    }
}

void step_sim(void) {
    activate_workers();
    parallel_work(symplectic_euler);
    init_grid();
    resolve_collisions();
    deactivate_workers();
}
