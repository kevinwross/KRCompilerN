#ifndef KRC_RUNTIME_HAL_H
#define KRC_RUNTIME_HAL_H

/* HAL — hardware abstraction layer interface (plan.md §14.1).
 *
 * The runtime calls these host-service functions to interact with the VM.
 * Each target provides its own implementation in krsrc/<target>/.
 */

#include <stddef.h>

/* Write a single character to the console. Returns the character, or -1 on error. */
int krc_hal_putchar(int ch);

/* Write n bytes from buf to fd. Returns bytes written, or -1 on error. */
int krc_hal_write(int fd, const void* buf, size_t n);

/* Read up to n bytes into buf from fd. Returns bytes read, or -1 on error. */
int krc_hal_read(int fd, void* buf, size_t n);

/* Open a file by name. Returns fd, or -1 on error. */
int krc_hal_open(const char* path);

/* Close an open file descriptor. Returns 0 on success, -1 on error. */
int krc_hal_close(int fd);

/* Exit the program with the given status. Does not return. */
void krc_hal_exit(int status);

/* Return the current time in seconds since the epoch, or (time_t)-1 on error. */
long krc_hal_time(void);

/* Allocate n bytes from the heap. Returns NULL on failure. */
void* krc_hal_alloc(size_t n);

/* Free a previously allocated block. */
void krc_hal_free(void* ptr);

#endif /* KRC_RUNTIME_HAL_H */

