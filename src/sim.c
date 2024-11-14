#include "sim.h"
#include <math.h>

void init_sim(void) {
    for (int i = 0; i < N_BALLS; i++) {
        balls_pos[i][0] = i % 16 + 0.5f;
        balls_pos[i][1] = i / 16 % 16 + 0.5f;
        balls_pos[i][2] = i / 256 % 16 + 0.5f;
    }
    for (int i = 0; i < N_BALLS; i++) {
        balls_vel[i][0] = 2.0 * drand48() - 1.0;
        balls_vel[i][1] = 2.0 * drand48() - 1.0;
        balls_vel[i][2] = 2.0 * drand48() - 1.0;
    }
}

void update_sim(void) {
    for (int i = 0; i < N_BALLS; i++) {
        glm_vec3_muladds(balls_vel[i], DT, balls_pos[i]); 
    }
    for (int i = 0; i < N_BALLS; i++) {
        for (int j = 0; j < 3; j++) {
            if (balls_pos[i][j] < RADIUS) {
                balls_pos[i][j] = RADIUS;
                balls_vel[i][j] = -balls_vel[i][j];
            } else if (balls_pos[i][j] > 16.0f - RADIUS) {
                balls_pos[i][j] = 16.0f - RADIUS;
                balls_vel[i][j] = -balls_vel[i][j];
            }
        }
        for (int j = 0; j < i; j++) {
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

