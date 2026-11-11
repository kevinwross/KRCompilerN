/* host_abi.c — Host ABI shims (plan.md §14.6).
 *
 * These functions bridge the KR runtime HAL to the host operating system
 * when running under a host simulator. They are not used when running on
 * bare KR hardware — the target-specific HAL implementation is used instead.
 *
 * Each function is a thin wrapper around the corresponding POSIX/Win32
 * call. On platforms where the host API is unavailable, these are stubs
 * that return error codes.
 */

#include "hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Console output through host stdout. */
int krc_hal_putchar(int ch) {
    return putc(ch, stdout);
}

/* Write through host write (write to fd). */
int krc_hal_write(int fd, const void* buf, size_t n) {
    /* For simplicity, write to stdout for fd==1, stderr for fd==2. */
    FILE* f = (fd == 2) ? stderr : stdout;
    return (int)fwrite(buf, 1, n, f);
}

/* Read through host read (read from fd). */
int krc_hal_read(int fd, void* buf, size_t n) {
    /* Only stdin (fd==0) is supported for reading. */
    if (fd != 0) return -1;
    return (int)fread(buf, 1, n, stdin);
}

/* Open a file through host fopen. */
int krc_hal_open(const char* path) {
    /* Host ABI shims don't track file descriptors; just test if the
     * file can be opened. Return a pseudo-fd. */
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fclose(f);
    return 0; /* pseudo-fd 0 means "opened successfully" */
}

/* Close a file through host fclose. */
int krc_hal_close(int fd) {
    (void)fd;
    return 0;
}

/* Exit through host exit. */
void krc_hal_exit(int status) {
    exit(status);
}

/* Time through host time(). */
long krc_hal_time(void) {
    return (long)time(NULL);
}

/* Allocate through host malloc. */
void* krc_hal_alloc(size_t n) {
    return malloc(n);
}

/* Free through host free. */
void krc_hal_free(void* ptr) {
    free(ptr);
}
