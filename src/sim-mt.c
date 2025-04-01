#include "sim.h"
#include "worker.h"
#include <math.h>
#include <string.h>

void init_positions(void);
void init_velocities(void);
void init_grid(void);
void resolve_collisions(void);

void init_sim(int argc, char **argv) {
    init_positions();
    init_velocities();
    if (argc >= 2) {
        n_workers = atoi(argv[1]);
    }
    create_workers();
}

static float fclampf(float v, float l, float h) {
    return fminf(fmaxf(v, l), h);
}

static void symplectic_euler_worker(int worker_idx) {
    int i = worker_idx * N_BALLS / n_workers;
    int n = (worker_idx + 1) * N_BALLS / n_workers;
    for (; i < n; i++) {
        sim.v[i].y -= 10.0f * DT;
        sim.x0[i] = sim.x[i];
        sim.x[i] = vec4_muladds(sim.v[i], DT, sim.x[i]);
        sim.x[i].x = fclampf(sim.x[i].x, GRID_MIN, GRID_MAX);
        sim.x[i].y = fclampf(sim.x[i].y, GRID_MIN, GRID_MAX);
        sim.x[i].z = fclampf(sim.x[i].z, GRID_MIN, GRID_MAX);
    }
}

static void newton_rasphon_worker(int worker_idx) {
    int i = worker_idx * N_BALLS / n_workers;
    int n = (worker_idx + 1) * N_BALLS / n_workers;
    for (; i < n; i++) {
        sim.v[i] = vec4_sub(sim.x[i], sim.x0[i]);
        sim.v[i] = vec4_scale(sim.v[i], SPS);
    }
}

static void symplectic_euler(void) {
    parallel_work(symplectic_euler_worker);
}

static void newton_rasphon(void) {
    parallel_work(newton_rasphon_worker);
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

static void resolve_tile_tile_collisions(int i0, int j0) {
    for (int i = i0; i >= 0; i = sim.nodes[i]) {
        if (i0 == j0) {
            for (int j = sim.nodes[i]; j >= 0; j = sim.nodes[j]) {
                resolve_ball_ball_collision(i, j);
            }
        } else {
            for (int j = j0; j >= 0; j = sim.nodes[j]) {
                resolve_ball_ball_collision(i, j);
            }
        }
    }
}

static void resolve_pair_collisions_worker(int worker_idx) {
    int lx = (GRID_LEN - sim.px - sim.nx + sim.ix - 1) / sim.ix;
    int xi = worker_idx * lx / n_workers * sim.ix + sim.px;
    int xf = (worker_idx + 1) * lx / n_workers * sim.ix + sim.px;
    int yi = sim.py;
    int yf = GRID_LEN - sim.ny; 
    int zi = sim.pz;
    int zf = GRID_LEN - sim.nz;
    int ix = sim.ix;
    int iy = sim.iy;
    int iz = sim.iz;
    for (int x = xi; x < xf; x += ix) {
        for (int y = yi; y < yf; y += iy) {
            for (int z = zi; z < zf; z += iz) {
                int i = sim.grid[x][y][z];
                int j = sim.grid[x + sim.tx][y + sim.ty][z + sim.tz];
                resolve_tile_tile_collisions(i, j);
            }
        }
    }
}

void resolve_pair_collisions(void) {
    parallel_work(resolve_pair_collisions_worker);
}

void step_sim(void) {
    activate_workers();
    symplectic_euler();
    init_grid();
    resolve_collisions();
    newton_rasphon();
    deactivate_workers();
}

void print_profile(void) {}
