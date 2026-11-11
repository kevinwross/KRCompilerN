#pragma once

void process_init(void);
int process_spawn(const char *name);
int process_exit(int pid, int status);
int process_reap(int pid);
void process_list(void);
