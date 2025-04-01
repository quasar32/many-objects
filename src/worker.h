#pragma once

extern int n_workers;

void create_workers(void);
void parallel_work(void(*work)(int));
void activate_workers(void);
void deactivate_workers(void);
