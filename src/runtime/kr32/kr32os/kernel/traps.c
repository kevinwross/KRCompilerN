#include "console.h"
#include "syscall.h"
#include "trap.h"

void kernel_leave_user(void);

void trap_dispatch(int vector, unsigned int *frame) {
    if ((frame[TRAP_FRAME_FLAGS] & TRAP_USER_MODE_BIT) == 0) {
        volatile struct syscall_scratch *scratch =
            (volatile struct syscall_scratch *)SYSCALL_SCRATCH_PHYS;
        scratch->result = USER_FAULT_STATUS;
        kernel_leave_user();
    }
    console_write("panic vector=");
    console_write_uint((unsigned int)vector);
    console_write(" pc=0x");
    console_write_uint(frame[TRAP_FRAME_PC]);
    console_write("\n");
}
