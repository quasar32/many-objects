#define N_BALLS 4096
#define RADIUS 0.4f
#define DIAMETER 0.8f
#define SPS 600
#define DT (1.0f / SPS)
#define GRID_LEN 32
#define GRID_MIN (RADIUS - GRID_LEN / 2)
#define GRID_MAX (GRID_LEN / 2 - RADIUS)

struct sim {
    float3 x[N_BALLS];
    float3 v[N_BALLS];
    float3 x0[N_BALLS];
    short nodes[N_BALLS];
    short grid[GRID_LEN][GRID_LEN][GRID_LEN];
    short tx, ty, tz;
    short px, py, pz;
    short nx, ny, nz;
    short ix, iy, iz;
};

static void resolve_ball_ball_collision(__global struct sim *sim, int i, int j) {
    float3 normal = sim->x[i] - sim->x[j];
    float d2 = dot(normal, normal);
    if (d2 > 0.0f && d2 < DIAMETER * DIAMETER) {
        float d = sqrt(d2);
        normal = normal / d;
        float corr = (DIAMETER - d) / 2.0f;
        float3 dx = normal * corr;
        sim->x[i] += dx;
        sim->x[j] -= dx;
        float vi = dot(sim->v[i], normal);
        float vj = dot(sim->v[j], normal);
        float3 dv = normal * (vi - vj);
        sim->v[i] -= dv;
        sim->v[j] += dv;
    }
}

void resolve_tile_tile_collisions(__global struct sim *sim, int i0, int j0) {
    for (int i = i0; i >= 0; i = sim->nodes[i]) {
        if (i0 == j0) {
            for (int j = sim->nodes[i]; j >= 0; j = sim->nodes[j]) {
                resolve_ball_ball_collision(sim, i, j);
            }
        } else {
            for (int j = j0; j >= 0; j = sim->nodes[j]) {
                resolve_ball_ball_collision(sim, i, j);
            }
        }
    }
}

__kernel void resolve_pair_collisions(__global struct sim *sim) {
    int i0 = get_global_id(0);
    int n0 = get_global_size(0);
    int lx = (GRID_LEN - sim->px - sim->nx + sim->ix - 1) / sim->ix;
    int xi = i0 * lx / n0 * sim->ix + sim->px;
    int xf = (i0 + 1) * lx / n0 * sim->ix + sim->px;
    //int yi = sim->py;
    //int yf = GRID_LEN - sim->ny; 
    int i1 = get_global_id(1);
    int n1  = get_global_size(1);
    int ly = (GRID_LEN - sim->py - sim->ny + sim->iy - 1) / sim->iy;
    int yi = i1 * ly / n1 * sim->iy + sim->py;
    int yf = (i1 + 1) * ly / n1 * sim->iy + sim->py;
    int i2 = get_global_id(2);
    int n2 = get_global_size(2);
    int lz = (GRID_LEN - sim->pz - sim->nz + sim->iz - 1) / sim->iz;
    int zi = i2 * lz / n1 * sim->iz + sim->pz;
    int zf = (i2 + 1) * lz / n2 * sim->iz + sim->pz;
    int ix = sim->ix;
    int iy = sim->iy;
    int iz = sim->iz;
    for (int x = xi; x < xf; x += ix) {
        for (int y = yi; y < yf; y += iy) {
            for (int z = zi; z < zf; z += iz) {
                int i = sim->grid[x][y][z];
                int j = sim->grid[x + sim->tx][y + sim->ty][z + sim->tz];
                resolve_tile_tile_collisions(sim, i, j);
            }
        }
    }
}

__kernel void symplectic_euler(__global struct sim *sim) {
    int worker_idx = get_global_id(0);
    int n_workers = get_global_size(0);
    int i = worker_idx * N_BALLS / n_workers;
    int n = (worker_idx + 1) * N_BALLS / n_workers;
    for (; i < n; i++) {
        sim->v[i].y -= 10.0f * DT;
        float3 x0 = sim->x[i];
        sim->x[i] += sim->v[i] * DT;
        sim->x[i].x = clamp(sim->x[i].x, GRID_MIN, GRID_MAX);
        sim->x[i].y = clamp(sim->x[i].y, GRID_MIN, GRID_MAX);
        sim->x[i].z = clamp(sim->x[i].z, GRID_MIN, GRID_MAX);
        sim->v[i] = (sim->x[i] - x0) * SPS;
    }
}
