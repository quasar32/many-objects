#pragma once

#define N_WORKERS 12

void create_workers(void);
void parallel_work(void(*work)(int));
void activate_workers(void);
void deactivate_workers(void);
