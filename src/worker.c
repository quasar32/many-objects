#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "worker.h"
#include "misc.h"

#define ALL_WORKERS ((1u << N_WORKERS) - 1u)

static pthread_mutex_t work_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t done_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t done_cond = PTHREAD_COND_INITIALIZER;
static pthread_t workers[N_WORKERS];
static unsigned workers_started = ALL_WORKERS;
static unsigned workers_done;
static void(*current_work)(int);

static void *consume_work(void *arg) {
    unsigned thread_idx = (uintptr_t) arg;
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
        if (workers_done == ALL_WORKERS) {
            pthread_cond_signal(&done_cond);
        }
        pthread_mutex_unlock(&done_mutex);
    }
    return NULL;
}

void create_workers(void) {
    for (uintptr_t i = 0; i < N_WORKERS; i++) {
        int err = pthread_create(&workers[i], NULL, consume_work, (void *) i);
        if (err) {
            die("pthread_create: %s\n", strerror(err));
        }
    }
}

void parallel_work(void(*work)(int)) {
    current_work = work;
    pthread_mutex_lock(&done_mutex);
    workers_done = 0;
    pthread_mutex_lock(&work_mutex);
    workers_started = 0;
    pthread_mutex_unlock(&work_mutex);
    pthread_cond_broadcast(&work_cond);
    while (workers_done != ALL_WORKERS) {
        pthread_cond_wait(&done_cond, &done_mutex);
    }
    pthread_mutex_unlock(&done_mutex);
}
