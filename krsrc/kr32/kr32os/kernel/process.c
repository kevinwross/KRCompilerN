#include "process.h"
#include "console.h"
#include "errno.h"

#define PROCESS_MAX 256
#define PROCESS_NAME_SIZE 32
#define PROCESS_STATE_FREE 0
#define PROCESS_STATE_RUNNABLE 1
#define PROCESS_STATE_ZOMBIE 2
#define PROCESS_STATE_REAPED 3

typedef char process_name_t[32]; /* PROCESS_NAME_SIZE */

struct Process {
    int used;
    int pid;
    int state;
    int status;
    process_name_t name;
};

typedef struct Process process_table_t[256]; /* PROCESS_MAX */
static process_table_t table;
static int next_pid = 1;

void process_init(void) {
    int index = 0;
    while (index < PROCESS_MAX) {
        table[index].used = 0;
        table[index].pid = 0;
        table[index].state = PROCESS_STATE_FREE;
        table[index].status = 0;
        table[index].name[0] = '\0';
        ++index;
    }
    next_pid = 1;
}

int process_spawn(const char *name) {
    int index = 0;
    while (index < PROCESS_MAX && table[index].used != 0) {
        ++index;
    }
    if (index >= PROCESS_MAX) {
        return -ERRNO_EAGAIN;
    }
    table[index].used = 1;
    table[index].pid = next_pid++;
    table[index].state = PROCESS_STATE_RUNNABLE;
    table[index].status = 0;
    int offset = 0;
    while (name[offset] != '\0' && offset < PROCESS_NAME_SIZE - 1) {
        table[index].name[offset] = name[offset];
        ++offset;
    }
    table[index].name[offset] = '\0';
    return table[index].pid;
}

int process_exit(int pid, int status) {
    int index = 0;
    while (index < PROCESS_MAX) {
        if (table[index].used != 0 && table[index].pid == pid) {
            table[index].state = PROCESS_STATE_ZOMBIE;
            table[index].status = status;
            return 0;
        }
        ++index;
    }
    return -ERRNO_ESRCH;
}

int process_reap(int pid) {
    int index = 0;
    while (index < PROCESS_MAX) {
        if (table[index].used != 0 && table[index].pid == pid) {
            table[index].state = PROCESS_STATE_REAPED;
            table[index].used = 0;
            return 0;
        }
        ++index;
    }
    return -ERRNO_ESRCH;
}

void process_list(void) {
    console_write("PID STATE NAME\n");
    int index = 0;
    while (index < PROCESS_MAX) {
        if (table[index].used != 0 &&
            table[index].state != PROCESS_STATE_REAPED) {
            console_write_uint((unsigned int)table[index].pid);
            console_write(" ");
            if (table[index].state == PROCESS_STATE_ZOMBIE) {
                console_write("zombie ");
            } else {
                console_write("running ");
            }
            console_write(table[index].name);
            console_putc('\n');
        }
        ++index;
    }
}
