#pragma once

/*
 * Shared syscall scratch page. User-mode stubs write the syscall vector and
 * result here before entering the kernel dispatcher.
 */
#define SYSCALL_SCRATCH_PHYS 0x0006F000U

struct syscall_scratch {
    volatile unsigned int result;
    volatile unsigned int vector;
};

#define SYSCALL_FD_STDOUT 1U
#define SYSCALL_FD_STDERR 2U

#define SYSCALL_EXIT 64U
#define SYSCALL_WRITE 65U
#define SYSCALL_READ 66U
#define SYSCALL_YIELD 67U
#define SYSCALL_GETPID 68U
#define SYSCALL_BRK 69U
