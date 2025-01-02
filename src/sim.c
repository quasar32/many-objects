#include "sim.h"
#include "worker.h"
#include <math.h>
#include <string.h>

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

static void resolve_ball_ball_collision(int i, int j) {
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

static void resolve_tile_tile_collisions(int i0, int j0) {
    for (int i = i0; i != UINT16_MAX; i = nodes[i]) {
        if (i0 == j0) {
            for (int j = nodes[i]; j != UINT16_MAX; j = nodes[j]) {
                resolve_ball_ball_collision(i, j);
            }
        } else {
            for (int j = j0; j != UINT16_MAX; j = nodes[j]) {
                resolve_ball_ball_collision(i, j);
            }
        }
    }
}

static int collision_idx;

static void resolve_pair_collisions(int worker_idx) {
    int dx = collision_idx % 3 - 1;
    int dy = collision_idx / 3 % 3 - 1;
    int dz = collision_idx / 9 - 1;
    int ix = dx && !dy && !dz ? 2 : 1;
    int iy = dy && !dz ? 2 : 1;
    int iz = dz ? 2 : 1;
    int nd = abs(dx) + abs(dy) + abs(dz);
    int tx = abs(dx);
    int ty = abs(dy);
    int tz = abs(dz);
    int x0, y0, x1, y1;
    switch (nd) {
    case 0:
    case 1:
        x0 = dx < 0;
        y0 = dy < 0;
        x1 = dx < 0;
        y1 = dy < 0;
        break;
    case 2:
        if (!dx) {
            ty = dy / dz;
            x0 = 0;
            y0 = ty < 0;
            x1 = 0;
            y1 = ty > 0;
        } else if (!dy) {
            tx = dx / dz;
            x0 = tx < 0;
            y0 = 0;
            x1 = tx > 0;
            y1 = 0;
        } else {
            tx = dx / dy;
            x0 = tx < 0;
            y0 = dy < 0;
            x1 = tx > 0;
            y1 = dy < 0;
        }
        break;
    default:
        tx = dx / dz;
        ty = dy / dz;
        x0 = tx < 0;
        y0 = ty < 0;
        x1 = tx > 0;
        y1 = ty > 0;
    }
    int z0 = dz < 0;
    int z1 = dz < 0;
    int lx = (GRID_LEN - x0 - x1 + ix - 1) / ix;
    int xi = worker_idx * lx / N_WORKERS * ix + x0;
    int xf = (worker_idx + 1) * lx / N_WORKERS * ix + x0;
    for (int x = xi; x < xf; x += ix) {
        for (int y = y0; y < GRID_LEN - y1; y += iy) {
            for (int z = z0; z < GRID_LEN - z1; z += iz) {
                int i = grid[x][y][z];
                int j = grid[x + tx][y + ty][z + tz];
                resolve_tile_tile_collisions(i, j);
            }
        }
    }
}

static void resolve_collisions(void) {
    for (int i = 0; i < 27; i++) {
        collision_idx = i;
        parallel_work(resolve_pair_collisions);
    }
}

void step_sim(void) {
    activate_workers();
    parallel_work(symplectic_euler);
    init_grid();
    resolve_collisions();
    deactivate_workers();
}
