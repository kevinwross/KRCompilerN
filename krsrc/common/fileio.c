/* fileio.c — File I/O on top of HAL (plan.md §14.6).
 *
 * Implements fopen, fclose, fread, fwrite, fprintf, fputc, fputs, fgetc.
 * Uses krc_hal_open/krc_hal_read/krc_hal_write/krc_hal_close.
 * A simple fixed-size FILE table is used (max 16 open files).
 */

#include "hal.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_OPEN_FILES 16

typedef struct {
    int fd;
    int flags;   /* 1 = read, 2 = write, 3 = read+write */
    int eof;
    int error;
} FILE_t;

static FILE_t file_table[MAX_OPEN_FILES];

static FILE_t* file_alloc(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (file_table[i].fd < 0) return &file_table[i];
    }
    return NULL;
}

void* fopen(const char* path, const char* mode) {
    int flags = 0;
    if (strchr(mode, 'r')) flags |= 1;
    if (strchr(mode, 'w')) flags |= 2;
    if (strchr(mode, 'a')) flags |= 2;

    int fd = krc_hal_open(path);
    if (fd < 0) return NULL;

    FILE_t* f = file_alloc();
    if (!f) {
        krc_hal_close(fd);
        return NULL;
    }

    f->fd = fd;
    f->flags = flags;
    f->eof = 0;
    f->error = 0;
    return f;
}

int fclose(void* stream) {
    FILE_t* f = (FILE_t*)stream;
    if (!f || f->fd < 0) return -1;
    int rc = krc_hal_close(f->fd);
    f->fd = -1;
    return rc;
}

size_t fread(void* ptr, size_t size, size_t count, void* stream) {
    FILE_t* f = (FILE_t*)stream;
    if (!f || f->fd < 0 || !(f->flags & 1)) return 0;

    size_t total = size * count;
    if (total == 0) return 0;
    if (f->eof) return 0;

    int rc = krc_hal_read(f->fd, ptr, total);
    if (rc < 0) {
        f->error = 1;
        return 0;
    }
    if (rc == 0) f->eof = 1;
    return (size_t)rc / size;
}

size_t fwrite(const void* ptr, size_t size, size_t count, void* stream) {
    FILE_t* f = (FILE_t*)stream;
    if (!f || f->fd < 0 || !(f->flags & 2)) return 0;

    size_t total = size * count;
    if (total == 0) return 0;

    int rc = krc_hal_write(f->fd, ptr, total);
    if (rc < 0) {
        f->error = 1;
        return 0;
    }
    return (size_t)rc / size;
}

int fputc(int c, void* stream) {
    unsigned char ch = (unsigned char)c;
    return fwrite(&ch, 1, 1, stream) == 1 ? c : -1;
}

int fputs(const char* s, void* stream) {
    (void)stream;
    return (int)fwrite(s, 1, strlen(s), stream);
}

int fgetc(void* stream) {
    FILE_t* f = (FILE_t*)stream;
    if (!f || f->fd < 0 || !(f->flags & 1)) return -1;
    if (f->eof) return -1;

    unsigned char ch;
    int rc = krc_hal_read(f->fd, &ch, 1);
    if (rc < 0) {
        f->error = 1;
        return -1;
    }
    if (rc == 0) {
        f->eof = 1;
        return -1;
    }
    return ch;
}

int fprintf(void* stream, const char* fmt, ...) {
    /* Simple fprintf: delegate to vsnprintf-style formatting. */
    (void)stream;
    (void)fmt;
    return -1; /* Not yet implemented */
}
