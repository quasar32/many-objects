#include "sim.h"
#include <math.h>
#include <string.h>
#include <pthread.h>

#define N_THREADS 8
#define ALL_THREADS ((1u << N_THREADS) - 1u)
#define TILEMAP_LEN 64
#define IS_POW2(v) (!((v) & ((v) - 1)) && (v))

vec3 balls_pos[N_BALLS];
vec3 balls_pos0[N_BALLS];
vec3 balls_vel[N_BALLS];

static pthread_t workers[N_THREADS];
static pthread_mutex_t work_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t done_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t done_cond = PTHREAD_COND_INITIALIZER;
static unsigned workers_started = ALL_THREADS;
static unsigned workers_done;
static void(*current_work)(int i);

static uint16_t tiles[N_BALLS];
static uint16_t tilemap[TILEMAP_LEN][TILEMAP_LEN][TILEMAP_LEN];

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

static void init_positions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        balls_pos[i][0] = i % 32 - 15.5f;
        balls_pos[i][1] = i / 32 % 32 - 15.5f;
        balls_pos[i][2] = i / 1024 - 15.5f;
    }
}

static void init_velocities(void) {
    for (int i = 0; i < N_BALLS; i++) {
        balls_vel[i][0] = 2.0 * drand48() - 1.0;
        balls_vel[i][1] = 2.0 * drand48() - 1.0;
        balls_vel[i][2] = 2.0 * drand48() - 1.0;
    }
}

static void *consume_work(void *arg) {
    int thread_idx = (int) (uintptr_t) arg;
    unsigned thread_flag = 1u << thread_idx;
    while (1) {
        pthread_mutex_lock(&work_mutex);
        while (workers_started & thread_flag) {
            pthread_cond_wait(&work_cond, &work_mutex);
        }
        workers_started |= thread_flag; 
        pthread_mutex_unlock(&work_mutex);
        current_work(thread_idx);
        pthread_mutex_lock(&done_mutex);
        workers_done |= thread_flag;
        if (workers_done == ALL_THREADS) {
            pthread_cond_signal(&done_cond);
        }
        pthread_mutex_unlock(&done_mutex);
    }
    return NULL;
}

static void init_workers(void) {
    for (int i = 0; i < N_THREADS; i++) {
        void *thread_idx = (void *) (uintptr_t) i;
        int err = pthread_create(&workers[i], NULL, consume_work, thread_idx);
        if (err) {
            fprintf(stderr, "pthread_create(%d): %s\n", err, strerror(err));
            exit(EXIT_FAILURE);
        }
    }
}

void init_sim(void) {
    init_workers();
    init_positions();
    init_velocities();
}

static void parallel_work(void(*work)(int)) {
    current_work = work;
    workers_started = 0;
    workers_done = 0;
    pthread_cond_broadcast(&work_cond);
    pthread_mutex_lock(&done_mutex);
    while (workers_done != ALL_THREADS) {
        pthread_cond_wait(&done_cond, &done_mutex);
    }
    pthread_mutex_unlock(&done_mutex);
}

static void symplectic_euler(int thread_idx) {
    int i = thread_idx * N_BALLS / N_THREADS;
    int n = (thread_idx + 1) * N_BALLS / N_THREADS;
    for (; i < n; i++) {
        glm_vec3_copy(balls_pos[i], balls_pos0[i]);
        balls_vel[i][1] -= 10.0f * DT;
        glm_vec3_muladds(balls_vel[i], DT, balls_pos[i]); 
        for (int j = 0; j < 3; j++) {
            if (balls_pos[i][j] < RADIUS - TILEMAP_LEN / 2) {
                balls_pos[i][j] = RADIUS - TILEMAP_LEN / 2;
            } else if (balls_pos[i][j] > TILEMAP_LEN / 2 - RADIUS) {
                balls_pos[i][j] = TILEMAP_LEN / 2 - RADIUS;
            }
        }
    }
}

static void clear_tiles(int thread_idx) {
    static_assert(IS_POW2(N_THREADS) && N_THREADS <= TILEMAP_LEN);
    memset(tilemap[thread_idx * TILEMAP_LEN / N_THREADS], 0xFF, 
            sizeof(tilemap) / N_THREADS);
}

static void add_nodes(int thread_idx) {
    int i = thread_idx * N_BALLS / N_THREADS;
    int n = (thread_idx + 1) * N_BALLS / N_THREADS;
    for (; i < n; i++) {
        int x = balls_pos[i][0] + TILEMAP_LEN / 2;
        int y = balls_pos[i][1] + TILEMAP_LEN / 2;
        int z = balls_pos[i][2] + TILEMAP_LEN / 2;
        tiles[i] = tilemap[x][y][z];
        tilemap[x][y][z] = i;
    }
}

static void newton_raphson(int thread_idx) {
    int i = thread_idx * N_BALLS / N_THREADS;
    int n = (thread_idx + 1) * N_BALLS / N_THREADS;
    for (; i < n; i++) {
        glm_vec3_sub(balls_pos[i], balls_pos0[i], balls_vel[i]);
        glm_vec3_divs(balls_vel[i], DT, balls_vel[i]);
    }
}

static void resolve_collisions(void) {
    for (int i = 0; i < N_BALLS; i++) {
        int bx = balls_pos[i][0] + TILEMAP_LEN / 2;
        int by = balls_pos[i][1] + TILEMAP_LEN / 2;
        int bz = balls_pos[i][2] + TILEMAP_LEN / 2;
        int x0 = max(bx - 1, 0); 
        int y0 = max(by - 1, 0);
        int z0 = max(bz - 1, 0);
        int x1 = min(bx + 1, TILEMAP_LEN - 1); 
        int y1 = min(by + 1, TILEMAP_LEN - 1);
        int z1 = min(bz + 1, TILEMAP_LEN - 1);
        for (int x = x0; x <= x1; x++) {
            for (int y = y0; y <= y1; y++) {
                for (int z = z0; z <= z1; z++) {
                    int j = tilemap[x][y][z]; 
                    for (; j != UINT16_MAX; j = tiles[j]) {
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

void step_sim(void) {
    parallel_work(symplectic_euler);
    parallel_work(clear_tiles);
    parallel_work(add_nodes);
    parallel_work(newton_raphson);
    resolve_collisions();
}

