#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "worker.h"
#include "misc.h"

#define ALL_WORKERS ((1u << N_WORKERS) - 1u)

static pthread_mutex_t active_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t active_cond = PTHREAD_COND_INITIALIZER;
static pthread_t workers[N_WORKERS - 1];
static unsigned workers_started = ALL_WORKERS;
static unsigned workers_done;
static void(*current_work)(int);
static int workers_active;

static int load(int *val) {
    return __atomic_load_n(val, __ATOMIC_SEQ_CST);
}

static void restloop(void) {
    pthread_mutex_lock(&active_mutex);
    while (!workers_active) {
        pthread_cond_wait(&active_cond, &active_mutex);
    }
    pthread_mutex_unlock(&active_mutex);
}

static void consume_work(unsigned worker_idx) {
    unsigned worker_flag = 1u << worker_idx;
    while (load(&workers_started) & worker_flag);
    __atomic_or_fetch(&workers_started, worker_flag, __ATOMIC_SEQ_CST);
    current_work(worker_idx);
    __atomic_or_fetch(&workers_done, worker_flag, __ATOMIC_SEQ_CST);
}

static void *worker_func(void *arg) {
    unsigned worker_idx = (uintptr_t) arg;
    while (1) {
        consume_work(worker_idx);
        restloop();
    }
    return NULL;
}

void create_workers(void) {
    for (uintptr_t i = 1; i < N_WORKERS; i++) {
        pthread_t *worker = &workers[i - 1];
        int err = pthread_create(worker, NULL, worker_func, (void *) i);
        if (err) {
            die("pthread_create: %s\n", strerror(err));
        }
    }
}

void parallel_work(void(*work)(int)) {
    current_work = work;
    workers_done = 0;
    __atomic_store_n(&workers_started, 0, __ATOMIC_SEQ_CST);
    consume_work(0);
    while (load(&workers_done) != ALL_WORKERS); 
}

void activate_workers(void) {
    pthread_mutex_lock(&active_mutex);
    workers_active = 1;
    pthread_mutex_unlock(&active_mutex);
    pthread_cond_broadcast(&active_cond);
}

void deactivate_workers(void) {
    pthread_mutex_lock(&active_mutex);
    workers_active = 0;
    pthread_mutex_unlock(&active_mutex);
}
