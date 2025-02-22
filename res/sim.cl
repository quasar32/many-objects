#define N_BALLS 4096
#define RADIUS 0.4f
#define DIAMETER 0.8f
#define GRID_LEN 32

struct sim {
    float3 x[N_BALLS];
    float3 x0[N_BALLS];
    float3 v[N_BALLS];
    short nodes[N_BALLS];
    short grid[GRID_LEN][GRID_LEN][GRID_LEN];
    short tx, ty, tz;
    short px, py, pz;
    short nx, ny, nz;
    short ix, iy, iz;
};

static void resolve_ball_ball_collision(__global struct sim *sim, int i, int j) {
    float3 normal = sim->x[i] - sim->x[j];
    float d2 = length(normal);
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
    int worker_idx = get_global_id(0);
    int n_workers = get_global_size(0);
    int lx = (GRID_LEN - sim->px - sim->nx + sim->ix - 1) / sim->ix;
    int xi = worker_idx * lx / n_workers * sim->ix + sim->px;
    int xf = (worker_idx + 1) * lx / n_workers * sim->ix + sim->px;
    int yi = sim->py;
    int yf = GRID_LEN - sim->ny; 
    int zi = sim->pz;
    int zf = GRID_LEN - sim->nz;
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
