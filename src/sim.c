#include "sim.h"
#include "worker.h"
#include <math.h>
#include <string.h>

struct sim sim;

void resolve_pair_collisions(void);

void init_positions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.x[i].x = i % 16 - GRID_LEN / 4 + 0.5f;
        sim.x[i].y = i / 16 % 16 - GRID_LEN / 4 + 0.5f;
        sim.x[i].z = i / 256 - GRID_LEN / 4 + 0.5f;
    }
}

void init_velocities(void) {
    for (int i = 0; i < N_BALLS; i++) {
        sim.v[i].x = 2.0 * drand48() - 1.0;
        sim.v[i].y = 2.0 * drand48() - 1.0;
        sim.v[i].z = 2.0 * drand48() - 1.0;
    }
}

void init_grid(void) {
    memset(sim.grid, 0xFF, sizeof(sim.grid));
    for (int i = 0; i < N_BALLS; i++) {
        int x = sim.x[i].x + GRID_LEN / 2;
        int y = sim.x[i].y + GRID_LEN / 2;
        int z = sim.x[i].z + GRID_LEN / 2;
        sim.nodes[i] = sim.grid[x][y][z];
        sim.grid[x][y][z] = i;
    }
}


void resolve_collisions(void) {
    for (int i = 0; i < 27; i++) {
        int dx = i % 3 - 1;
        int dy = i / 3 % 3 - 1;
        int dz = i / 9 - 1;
        sim.ix = dx && !dy && !dz ? 2 : 1;
        sim.iy = dy && !dz ? 2 : 1;
        sim.iz = dz ? 2 : 1;
        int nd = abs(dx) + abs(dy) + abs(dz);
        sim.tx = abs(dx);
        sim.ty = abs(dy);
        sim.tz = abs(dz);
        switch (nd) {
        case 0:
        case 1:
            sim.px = dx < 0;
            sim.py = dy < 0;
            sim.nx = dx < 0;
            sim.ny = dy < 0;
            break;
        case 2:
            if (!dx) {
                sim.ty = dy / dz;
                sim.px = 0;
                sim.py = sim.ty < 0;
                sim.nx = 0;
                sim.ny = sim.ty > 0;
            } else if (!dy) {
                sim.tx = dx / dz;
                sim.px = sim.tx < 0;
                sim.py = 0;
                sim.nx = sim.tx > 0;
                sim.ny = 0;
            } else {
                sim.tx = dx / dy;
                sim.px = sim.tx < 0;
                sim.py = dy < 0;
                sim.nx = sim.tx > 0;
                sim.ny = dy < 0;
            }
            break;
        default:
            sim.tx = dx / dz;
            sim.ty = dy / dz;
            sim.px = sim.tx < 0;
            sim.py = sim.ty < 0;
            sim.nx = sim.tx > 0;
            sim.ny = sim.ty > 0;
        }
        sim.pz = dz < 0;
        sim.nz = dz < 0;
        resolve_pair_collisions();
    }
}
