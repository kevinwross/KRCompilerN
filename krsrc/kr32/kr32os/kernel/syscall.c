#include "console.h"
#include "errno.h"
#include "memory.h"
#include "syscall.h"
#include "user_memory.h"

void sys_exit(int status) {
    volatile struct syscall_scratch *scratch =
        (volatile struct syscall_scratch *)SYSCALL_SCRATCH_PHYS;
    scratch->result = (unsigned int)status;
}

int sys_write(int descriptor, const char *buffer, int length) {
    if (descriptor != SYSCALL_FD_STDOUT &&
        descriptor != SYSCALL_FD_STDERR) {
        return -ERRNO_EBADF;
    }
    if (length < 0) {
        return -ERRNO_EINVAL;
    }
    const mem_virt_t address = (mem_virt_t)buffer;
    if (address < USER_CODE_VA || address >= USER_STACK_TOP_VA) {
        return -ERRNO_EFAULT;
    }
    int index = 0;
    while (index < length) {
        const mem_virt_t current = address + (mem_virt_t)index;
        const mem_phys_t physical = user_virt_to_phys(current);
        if (physical == MEM_PHYS_NONE) {
            return -ERRNO_EFAULT;
        }
        console_putc(*(const char *)mem_phys_to_virt(physical));
        ++index;
    }
    return length;
}

int sys_read(int descriptor, char *buffer, int length) {
    (void)descriptor;
    (void)buffer;
    (void)length;
    return 0;
}

int sys_yield(void) {
    return 0;
}

int sys_getpid(void) {
    return 1;
}

int sys_brk(unsigned int address) {
    (void)address;
    return 0;
}

int sys_dispatch(int argument0, int argument1, int argument2) {
    volatile struct syscall_scratch *scratch =
        (volatile struct syscall_scratch *)SYSCALL_SCRATCH_PHYS;
    if (scratch->vector == SYSCALL_READ) {
        return sys_read(argument0, (char *)argument1, argument2);
    }
    if (scratch->vector == SYSCALL_YIELD) {
        return sys_yield();
    }
    if (scratch->vector == SYSCALL_GETPID) {
        return sys_getpid();
    }
    if (scratch->vector == SYSCALL_BRK) {
        return sys_brk((unsigned int)argument0);
    }
    return -ERRNO_ENOSYS;
}
