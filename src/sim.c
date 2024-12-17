#include "sim.h"
#include <math.h>
#include <string.h>

static uint16_t nodes[N_BALLS];
static uint16_t tiles[16][16][16];

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

void init_sim(void) {
    for (int i = 0; i < N_BALLS; i++) {
        balls_pos[i][0] = i % 16 - 7.5f;
        balls_pos[i][1] = i / 16 % 16 - 7.5f;
        balls_pos[i][2] = i / 256 % 16 - 7.5f;
    }
    for (int i = 0; i < N_BALLS; i++) {
        balls_vel[i][0] = 2.0 * drand48() - 1.0;
        balls_vel[i][1] = 2.0 * drand48() - 1.0;
        balls_vel[i][2] = 2.0 * drand48() - 1.0;
    }
}

void update_sim(void) {
    memset(tiles, 0xFF, sizeof(tiles));
    for (int i = 0; i < N_BALLS; i++) {
        int x = balls_pos[i][0] + 8.0f;
        int y = balls_pos[i][1] + 8.0f;
        int z = balls_pos[i][2] + 8.0f;
        nodes[i] = tiles[x][y][z];
        tiles[x][y][z] = i;
    }
    for (int i = 0; i < N_BALLS; i++) {
        glm_vec3_copy(balls_pos[i], balls_pos0[i]);
        balls_vel[i][1] -= 10.0f * DT;
        glm_vec3_muladds(balls_vel[i], DT, balls_pos[i]); 
    }
    for (int i = 0; i < N_BALLS; i++) {
        for (int j = 0; j < 3; j++) {
            if (balls_pos[i][j] < RADIUS - 8.0f) {
                balls_pos[i][j] = RADIUS - 8.0f;
            } else if (balls_pos[i][j] > 8.0f - RADIUS) {
                balls_pos[i][j] = 8.0f - RADIUS;
            }
        }
    }
    for (int i = 0; i < N_BALLS; i++) {
        glm_vec3_sub(balls_pos[i], balls_pos0[i], balls_vel[i]);
        glm_vec3_divs(balls_vel[i], DT, balls_vel[i]);
    }
    for (int i = 0; i < N_BALLS; i++) {
        int bx = balls_pos[i][0] + 8.0f;
        int by = balls_pos[i][1] + 8.0f;
        int bz = balls_pos[i][2] + 8.0f;
        int x0 = max(bx - 1, 0); 
        int y0 = max(by - 1, 0);
        int z0 = max(bz - 1, 0);
        int x1 = min(bx + 1, 15); 
        int y1 = min(by + 1, 15);
        int z1 = min(bz + 1, 15);
        for (int x = x0; x <= x1; x++) {
            for (int y = y0; y <= y1; y++) {
                for (int z = z0; z <= z1; z++) {
                    int j = tiles[x][y][z]; 
                    for (; j != UINT16_MAX; j = nodes[j]) {
                        if (j >= i)
                            continue;
                        vec3 normal;
                        glm_vec3_sub(balls_pos[i], balls_pos[j], normal);
                        float d2 = glm_vec3_norm2(normal); 
                        if (d2 > 0.0f && d2 < DIAMETER * DIAMETER) {
                            float d = sqrtf(d2);
                            glm_vec3_divs(normal, d, normal);
                            float corr = (DIAMETER - d) / 2.0f;
                            glm_vec3_muladds(normal, corr, balls_pos[i]);
                            glm_vec3_mulsubs(normal, corr, balls_pos[j]);
                            float vi = glm_vec3_dot(balls_vel[i], normal);
                            float vj = glm_vec3_dot(balls_vel[j], normal);
                            glm_vec3_muladds(normal, vj - vi, balls_vel[i]); 
                            glm_vec3_muladds(normal, vi - vj, balls_vel[j]); 
                        }
                    }
                }
            }
        }
    }
}

