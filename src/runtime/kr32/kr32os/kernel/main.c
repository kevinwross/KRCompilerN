#include "console.h"
#include "memory.h"
#include "timer.h"

void shell_run(void);
void traps_install(void);
void kernel_enable_interrupts(void);
void user_install_syscalls(void);
void process_init(void);
int run_init(void);

void kmain(void) {
    console_write("KR32OS M4\n");
    traps_install();
    user_install_syscalls();
    memory_init();
    process_init();
    timer_init();
    kernel_enable_interrupts();
    int status = run_init();
    if (status >= 0) {
        console_write("init exited ");
        console_write_uint((unsigned int)status);
        console_putc('\n');
    }
    shell_run();
}
