/*
 * KR16 C runtime: system-service wrappers and a small stdio layer.
 *
 * This file is compiled by kr16 cc and linked after libc.s. It deliberately
 * uses the fixed 16-byte host-call block described in systemservices.md.
 */

#include <kr16/systemservices.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct FILE FILE;

struct FILE {
    int fd;
    int readable;
    int writable;
    int eof;
    int error;
};

#define KR16_FILE_MAX 8
#define KR16_EOF (-1)

struct sys_call_block {
    sys_u16 status;
    sys_u16 result[3];
    sys_u16 arg[4];
};

extern int __host_call(int service, void *block);

static void block_clear(struct sys_call_block *block) {
    block->status = 0;
    block->result[0] = 0;
    block->result[1] = 0;
    block->result[2] = 0;
    block->arg[0] = 0;
    block->arg[1] = 0;
    block->arg[2] = 0;
    block->arg[3] = 0;
}

int sys_version(const struct sys_version_args *args,
                struct sys_version_result *result) {
    struct sys_call_block block;
    int status;
    (void)args;
    block_clear(&block);
    status = __host_call(0, &block);
    if (result != 0) {
        result->abi = block.result[0];
        result->capabilities = block.result[1];
        result->reserved = block.result[2];
    }
    return status;
}

int sys_open(const struct sys_open_args *args,
             struct sys_open_result *result) {
    struct sys_call_block block;
    int status;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)(uintptr_t)args->path;
    block.arg[1] = args->flags;
    block.arg[2] = args->mode;
    status = __host_call(1, &block);
    if (result != 0) {
        result->fd = (sys_fd)block.result[0];
    }
    return status;
}

int sys_close(const struct sys_close_args *args) {
    struct sys_call_block block;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)args->fd;
    return __host_call(2, &block);
}

int sys_read(const struct sys_read_args *args,
             struct sys_read_result *result) {
    struct sys_call_block block;
    int status;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)args->fd;
    block.arg[1] = (sys_u16)(uintptr_t)args->buffer;
    block.arg[2] = args->count;
    status = __host_call(3, &block);
    if (result != 0) {
        result->bytes = block.result[0];
    }
    return status;
}

int sys_write(const struct sys_write_args *args,
              struct sys_write_result *result) {
    struct sys_call_block block;
    int status;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)args->fd;
    block.arg[1] = (sys_u16)(uintptr_t)args->buffer;
    block.arg[2] = args->count;
    status = __host_call(4, &block);
    if (result != 0) {
        result->bytes = block.result[0];
    }
    return status;
}

int sys_seek(const struct sys_seek_args *args,
             struct sys_seek_result *result) {
    struct sys_call_block block;
    int status;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)args->fd;
    block.arg[1] = (sys_u16)args->offset.lo;
    block.arg[2] = (sys_u16)args->offset.hi;
    block.arg[3] = args->whence;
    status = __host_call(5, &block);
    if (result != 0) {
        result->offset.lo = (sys_u16)block.result[0];
        result->offset.hi = (sys_u16)block.result[1];
    }
    return status;
}

int sys_stat(const struct sys_stat_args *args) {
    struct sys_call_block block;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)(uintptr_t)args->path;
    block.arg[1] = (sys_u16)(uintptr_t)args->buffer;
    return __host_call(6, &block);
}

int sys_unlink(const struct sys_unlink_args *args) {
    struct sys_call_block block;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)(uintptr_t)args->path;
    return __host_call(7, &block);
}

int sys_rename(const struct sys_rename_args *args) {
    struct sys_call_block block;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)(uintptr_t)args->old_path;
    block.arg[1] = (sys_u16)(uintptr_t)args->new_path;
    return __host_call(8, &block);
}

int sys_mkdir(const struct sys_mkdir_args *args) {
    struct sys_call_block block;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)(uintptr_t)args->path;
    return __host_call(9, &block);
}

int sys_readdir(const struct sys_readdir_args *args,
                struct sys_readdir_result *result) {
    struct sys_call_block block;
    int status;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)args->fd;
    block.arg[1] = (sys_u16)(uintptr_t)args->buffer;
    block.arg[2] = args->count;
    status = __host_call(10, &block);
    if (result != 0) {
        result->count = block.result[0];
    }
    return status;
}

int sys_time(const struct sys_time_args *args,
             struct sys_time_result *result) {
    struct sys_call_block block;
    int status;
    (void)args;
    block_clear(&block);
    status = __host_call(11, &block);
    if (result != 0) {
        result->milliseconds.lo = (sys_u16)block.result[0];
        result->milliseconds.hi = (sys_u16)block.result[1];
    }
    return status;
}

int sys_random(const struct sys_random_args *args,
               struct sys_random_result *result) {
    struct sys_call_block block;
    int status;
    (void)args;
    block_clear(&block);
    status = __host_call(12, &block);
    if (result != 0) {
        result->value = (sys_u16)block.result[0];
    }
    return status;
}

int sys_fsinfo(const struct sys_fsinfo_args *args) {
    struct sys_call_block block;
    if (args == 0) {
        return SYS_EINVAL;
    }
    block_clear(&block);
    block.arg[0] = (sys_u16)(uintptr_t)args->buffer;
    return __host_call(13, &block);
}

/* stdio layer */

static FILE files[KR16_FILE_MAX];
static int files_used[KR16_FILE_MAX];

static FILE *file_open_table_slot(void) {
    int i;
    for (i = 0; i < KR16_FILE_MAX; ++i) {
        if (!files_used[i]) {
            files_used[i] = 1;
            return &files[i];
        }
    }
    return 0;
}

static void file_close_table_slot(FILE *stream) {
    int i;
    for (i = 0; i < KR16_FILE_MAX; ++i) {
        if (&files[i] == stream) {
            files_used[i] = 0;
            return;
        }
    }
}

FILE *fopen(const char *path, const char *mode) {
    struct sys_open_args args;
    struct sys_open_result result;
    FILE *stream;
    int fd;

    args.path = (char *)path;
    args.flags = SYS_O_RDONLY;
    args.mode = 0;

    if (strcmp(mode, "w") == 0 || strcmp(mode, "w+") == 0) {
        args.flags = SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC;
    } else if (strcmp(mode, "r+") == 0) {
        args.flags = SYS_O_RDWR;
    } else if (strcmp(mode, "a") == 0) {
        args.flags = SYS_O_WRONLY | SYS_O_CREAT | SYS_O_APPEND;
    }

    if (sys_open(&args, &result) != SYS_OK) {
        return 0;
    }
    fd = result.fd;

    stream = file_open_table_slot();
    if (stream == 0) {
        struct sys_close_args close_args;
        close_args.fd = fd;
        sys_close(&close_args);
        return 0;
    }

    stream->fd = fd;
    stream->readable = (mode[0] == 'r') || strchr(mode, '+') != 0;
    stream->writable = (mode[0] == 'w') || (mode[0] == 'a') || strchr(mode, '+') != 0;
    stream->eof = 0;
    stream->error = 0;
    return stream;
}

int fclose(FILE *stream) {
    struct sys_close_args close_args;
    if (stream == 0) {
        return KR16_EOF;
    }
    close_args.fd = stream->fd;
    if (sys_close(&close_args) != SYS_OK) {
        return KR16_EOF;
    }
    file_close_table_slot(stream);
    return 0;
}

int fgetc(FILE *stream) {
    unsigned char ch;
    struct sys_read_args args;
    struct sys_read_result result;

    if (stream == 0 || !stream->readable) {
        return KR16_EOF;
    }

    args.fd = stream->fd;
    args.buffer = &ch;
    args.count = 1;

    if (sys_read(&args, &result) != SYS_OK || result.bytes == 0) {
        stream->eof = 1;
        return KR16_EOF;
    }
    return ch;
}

int fputc(int character, FILE *stream) {
    unsigned char ch = (unsigned char)character;
    struct sys_write_args args;
    struct sys_write_result result;

    if (stream == 0 || !stream->writable) {
        return KR16_EOF;
    }

    args.fd = stream->fd;
    args.buffer = &ch;
    args.count = 1;

    if (sys_write(&args, &result) != SYS_OK || result.bytes == 0) {
        stream->error = 1;
        return KR16_EOF;
    }
    return character;
}

int fputs(const char *s, FILE *stream) {
    int i;
    for (i = 0; s[i] != '\0'; ++i) {
        if (fputc(s[i], stream) == KR16_EOF) {
            return KR16_EOF;
        }
    }
    return 0;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
    size_t total = size * count;
    size_t done = 0;
    struct sys_read_args args;
    struct sys_read_result result;

    if (stream == 0 || !stream->readable || count == 0) {
        return 0;
    }

    args.fd = stream->fd;
    args.buffer = ptr;
    args.count = (sys_u16)total;

    if (sys_read(&args, &result) != SYS_OK) {
        stream->error = 1;
        return 0;
    }
    done = result.bytes;
    if (done < total) {
        stream->eof = 1;
    }
    return done / size;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    size_t total = size * count;
    size_t done = 0;
    struct sys_write_args args;
    struct sys_write_result result;

    if (stream == 0 || !stream->writable || count == 0) {
        return 0;
    }

    args.fd = stream->fd;
    args.buffer = (void *)ptr;
    args.count = (sys_u16)total;

    if (sys_write(&args, &result) != SYS_OK) {
        stream->error = 1;
        return 0;
    }
    done = result.bytes;
    return done / size;
}

int fprintf(FILE *stream, const char *format, ...) {
    (void)stream;
    (void)format;
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    (void)stream;
    (void)offset;
    (void)whence;
    return 0;
}

int feof(FILE *stream) {
    return stream != 0 ? stream->eof : 0;
}

int ferror(FILE *stream) {
    return stream != 0 ? stream->error : 0;
}

void clearerr(FILE *stream) {
    if (stream != 0) {
        stream->eof = 0;
        stream->error = 0;
    }
}

int remove(const char *path) {
    struct sys_unlink_args args;
    if (path == 0) {
        return KR16_EOF;
    }
    args.path = (char *)path;
    return sys_unlink(&args) == SYS_OK ? 0 : KR16_EOF;
}

int rename(const char *old_path, const char *new_path) {
    struct sys_rename_args args;
    if (old_path == 0 || new_path == 0) {
        return KR16_EOF;
    }
    args.old_path = (char *)old_path;
    args.new_path = (char *)new_path;
    return sys_rename(&args) == SYS_OK ? 0 : KR16_EOF;
}
