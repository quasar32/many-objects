#pragma once

#define N_WORKERS 4

void create_workers(void);
void parallel_work(void(*work)(int));
