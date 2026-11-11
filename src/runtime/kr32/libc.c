
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

long strtol(const char *text, char **end, int base);
long atol(const char *text);
int sys_time(void);
typedef int (*kr32_compare_fn)(const void *, const void *);
void qsort(void *base, int count, int size, kr32_compare_fn compare);
void *bsearch(const void *key, const void *base, int count, int size, kr32_compare_fn compare);

int putchar(int c);
int getchar(void);
void exit(int code);

/* Provided by the KR32 runtime/loader for the heap bounds. */
extern void *__kr32_heap_start(void);
extern void *__kr32_heap_end(void);

#ifdef errno
#undef errno
#endif

int errno = 0;

#define EPERM 1
#define ENOENT 2
#define EIO 5
#define EBADF 9
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define EINVAL 22
#define ENOSPC 28
#define ENOSYS 38
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define BUFSIZ 1024

struct stat {
    unsigned int st_size;
    unsigned int st_mode;
    unsigned int st_mtime;
    unsigned int st_reserved;
};

int open(const char *path, int flags);
int close(int fd);
int read(int fd, void *buffer, int count);
int write(int fd, const void *buffer, int count);
int seek(int fd, int offset, int whence);
int unlink(const char *path);
int rename(const char *old_path, const char *new_path);
int kr32_getcwd(char *buffer, int size);
int kr32_chdir(const char *path);
int stat(const char *path, struct stat *status);

void *memcpy(void *destination, const void *source, int count);
void *memset(void *destination, int value, int count);

#define EOF (-1)
#define KR32_FILE_READ 1
#define KR32_FILE_WRITE 2
#define KR32_FILE_EOF 4
#define KR32_FILE_ERROR 8

struct kr32_file {
    int descriptor;
    int flags;
    int ungot;
    int owns_descriptor;
    const char *memory;
    int memory_position;
    int memory_mode;
    unsigned char *buffer;
    int buffer_capacity;
    int buffer_length;
    int buffer_mode;
    int buffer_owned;
    unsigned char *input_buffer;
    int input_capacity;
    int input_begin;
    int input_end;
    int input_owned;
};

typedef struct kr32_file FILE;



static struct kr32_file stdin_object;
static struct kr32_file stdout_object;
static struct kr32_file stderr_object;

struct kr32_file *stdin = 0;
struct kr32_file *stdout = 0;
struct kr32_file *stderr = 0;

void __kr32_stdio_init(void) {
    stdin_object.descriptor = 0;
    stdin_object.flags = KR32_FILE_READ;
    stdin_object.ungot = -1;
    stdin_object.owns_descriptor = 0;
    stdout_object.descriptor = 1;
    stdout_object.flags = KR32_FILE_WRITE;
    stdout_object.ungot = -1;
    stdout_object.owns_descriptor = 0;
    stderr_object.descriptor = 2;
    stderr_object.flags = KR32_FILE_WRITE;
    stderr_object.ungot = -1;
    stderr_object.owns_descriptor = 0;
    stdin = &stdin_object;
    stdout = &stdout_object;
    stderr = &stderr_object;
}

void clearerr(struct kr32_file *stream);

struct heap_block {
    uintptr_t size;
    uintptr_t is_free;
    uintptr_t next;
    uintptr_t prev;
};

static struct heap_block *heap_free_list = 0;
static uintptr_t heap_begin = 0;
static uintptr_t heap_end_address = 0;
static int heap_ready = 0;

/* Live heap accounting, exported so the debugger can display it. */
uintptr_t heap_allocated_blocks = 0;
uintptr_t heap_free_blocks = 0;
static int heap_lock_state = 0;

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchange)

static void heap_lock_acquire(void) {
    volatile long *state = (volatile long *)&heap_lock_state;
    long expected = 0;
    while (_InterlockedCompareExchange(state, 1, 0) != 0) {
        expected = 0;
        /* spin */
    }
}

static void heap_lock_release(void) {
    volatile long *state = (volatile long *)&heap_lock_state;
    _InterlockedExchange(state, 0);
}
#else
static void heap_lock_acquire(void) {
    int expected = 0;
    while (!__atomic_compare_exchange_n(&heap_lock_state, &expected, 1, 0, 0, 0)) {
        expected = 0;
    }
}

static void heap_lock_release(void) {
    __atomic_store_n(&heap_lock_state, 0, 0);
}
#endif

unsigned long long __kr32_ulldiv(unsigned long long left,
                                  unsigned long long right) {
    unsigned long long quotient = 0;
    unsigned long long remainder = 0;
    int bit = 63;
    while (bit >= 0) {
        remainder = (remainder << 1) |
                    ((left >> bit) & (unsigned long long)1);
        if (remainder >= right) {
            remainder = remainder - right;
            quotient = quotient | ((unsigned long long)1 << bit);
        }
        bit = bit - 1;
    }
    return quotient;
}

unsigned long long __kr32_ulmod(unsigned long long left,
                                 unsigned long long right) {
    unsigned long long remainder = 0;
    int bit = 63;
    while (bit >= 0) {
        remainder = (remainder << 1) |
                    ((left >> bit) & (unsigned long long)1);
        if (remainder >= right) {
            remainder = remainder - right;
        }
        bit = bit - 1;
    }
    return remainder;
}

long long __kr32_lldiv(long long left, long long right) {
    int negative = 0;
    unsigned long long uleft = left;
    unsigned long long uright = right;
    if (left < 0) {
        uleft = (unsigned long long)0 - uleft;
        negative = 1;
    }
    if (right < 0) {
        uright = (unsigned long long)0 - uright;
        negative = 1 - negative;
    }
    unsigned long long result = __kr32_ulldiv(uleft, uright);
    if (negative) {
        result = (unsigned long long)0 - result;
    }
    return result;
}

long long __kr32_llmod(long long left, long long right) {
    int negative = 0;
    unsigned long long uleft = left;
    unsigned long long uright = right;
    if (left < 0) {
        uleft = (unsigned long long)0 - uleft;
        negative = 1;
    }
    if (right < 0) {
        uright = (unsigned long long)0 - uright;
    }
    unsigned long long result = __kr32_ulmod(uleft, uright);
    if (negative) {
        result = (unsigned long long)0 - result;
    }
    return result;
}

static unsigned int heap_align(unsigned int value, unsigned int alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static unsigned int heap_block_size(unsigned int payload) {
    unsigned int total = payload + sizeof(struct heap_block) + 4;
    unsigned int minimum = sizeof(struct heap_block) + 12;
    total = heap_align(total, 8);
    if (total < minimum) {
        total = minimum;
    }
    return total;
}

static unsigned int heap_footer(struct heap_block *block) {
    unsigned int address = (uintptr_t)block + block->size - 4;
    return *(uintptr_t *)address;
}

static void heap_set_footer(struct heap_block *block) {
    unsigned int address = (uintptr_t)block + block->size - 4;
    *(uintptr_t *)address = block->size;
}

static struct heap_block *heap_next(struct heap_block *block) {
    unsigned int address = (uintptr_t)block + block->size;
    if (address + sizeof(struct heap_block) > heap_end_address) {
        return 0;
    }
    return (struct heap_block *)address;
}

static void heap_init(void) {
    uintptr_t begin = (uintptr_t)__kr32_heap_start();
    uintptr_t end = (uintptr_t)__kr32_heap_end();
    begin = heap_align(begin, 8);
    end = end & ~7;
    heap_begin = begin;
    heap_end_address = end;
    if (begin + sizeof(struct heap_block) + 12 > end) {
        return;
    }
    struct heap_block *block = (struct heap_block *)begin;
    block->size = end - begin;
    block->is_free = 1;
    block->next = 0;
    block->prev = 0;
    heap_set_footer(block);
    heap_free_list = block;
    heap_allocated_blocks = 0;
    heap_free_blocks = 1;
    heap_ready = 1;
}

static void heap_remove(struct heap_block *block) {
    struct heap_block *previous = (struct heap_block *)block->prev;
    struct heap_block *next = (struct heap_block *)block->next;
    if (heap_free_blocks > 0) {
        heap_free_blocks = heap_free_blocks - 1;
    }
    if (previous != 0) {
        previous->next = (uintptr_t)next;
    } else {
        heap_free_list = next;
    }
    if (next != 0) {
        next->prev = (uintptr_t)previous;
    }
    block->next = 0;
    block->prev = 0;
}

static void heap_insert(struct heap_block *block) {
    struct heap_block *previous = 0;
    struct heap_block *current = heap_free_list;
    heap_free_blocks = heap_free_blocks + 1;
    while (current != 0 && (uintptr_t)current < (uintptr_t)block) {
        previous = current;
        current = (struct heap_block *)current->next;
    }
    block->prev = (uintptr_t)previous;
    block->next = (uintptr_t)current;
    if (previous != 0) {
        previous->next = (uintptr_t)block;
    } else {
        heap_free_list = block;
    }
    if (current != 0) {
        current->prev = (uintptr_t)block;
    }
}

static void *heap_alloc_locked(int size) {
    if (size <= 0) {
        return 0;
    }
    if (!heap_ready) {
        heap_init();
    }
    if (!heap_ready) {
        return 0;
    }
    unsigned int needed = heap_block_size((uintptr_t)size);
    struct heap_block *best = 0;
    struct heap_block *current = heap_free_list;
    while (current != 0) {
        if (current->is_free && current->size >= needed) {
            if (best == 0 || current->size < best->size) {
                best = current;
                if (current->size == needed) {
                    break;
                }
            }
        }
        current = (struct heap_block *)(uintptr_t)current->next;
    }
    if (best == 0) {
        return 0;
    }
    heap_remove(best);
    unsigned int minimum = sizeof(struct heap_block) + 12;
    if (best->size >= needed + minimum) {
        struct heap_block *remainder =
            (struct heap_block *)((uintptr_t)best + needed);
        remainder->size = best->size - needed;
        remainder->is_free = 1;
        remainder->next = 0;
        remainder->prev = 0;
        heap_set_footer(remainder);
        best->size = needed;
        heap_insert(remainder);
    }
    best->is_free = 0;
    heap_allocated_blocks = heap_allocated_blocks + 1;
    heap_set_footer(best);
    return (void *)((uintptr_t)best + sizeof(struct heap_block));
}

static void heap_free_locked(void *pointer) {
    if (pointer == 0) {
        return;
    }
    struct heap_block *block =
        (struct heap_block *)((uintptr_t)pointer - sizeof(struct heap_block));
    if (block->is_free) {
        return;
    }
    block->is_free = 1;
    if (heap_allocated_blocks > 0) {
        heap_allocated_blocks = heap_allocated_blocks - 1;
    }

    struct heap_block *next = heap_next(block);
    if (next != 0 && next->is_free) {
        heap_remove(next);
        block->size = block->size + next->size;
        heap_set_footer(block);
    }

    if ((uintptr_t)block > heap_begin) {
        unsigned int previous_size = *(uintptr_t *)((uintptr_t)block - 4);
        struct heap_block *previous =
            (struct heap_block *)((uintptr_t)block - previous_size);
        if (previous->is_free) {
            heap_remove(previous);
            previous->size = previous->size + block->size;
            heap_set_footer(previous);
            block = previous;
        }
    }
    heap_insert(block);
}

void *malloc(int size) {
    heap_lock_acquire();
    void *result = heap_alloc_locked(size);
    heap_lock_release();
    return result;
}

void free(void *pointer) {
    heap_lock_acquire();
    heap_free_locked(pointer);
    heap_lock_release();
}

void *__kr32_new_alloc(int size, int alignment) {
    if (size <= 0) {
        size = 1;
    }
    if (alignment < 4 || (alignment & (alignment - 1)) != 0) {
        alignment = 4;
    }
    unsigned int limit = 0x7FFFFFFFu;
    if ((uintptr_t)size > limit - (uintptr_t)alignment - 12U) {
        return 0;
    }
    unsigned int total = (uintptr_t)size + (uintptr_t)alignment + 12U;
    struct heap_block *base = (struct heap_block *)malloc((int)total);
    if (base == 0) {
        return 0;
    }
    unsigned int address = (uintptr_t)base + 12U;
    address = (address + (uintptr_t)alignment - 1U) & ~((uintptr_t)alignment - 1U);
    *(uintptr_t *)(address - 12U) = (uintptr_t)base;
    return (void *)address;
}

void __kr32_new_free(void *pointer) {
    if (pointer == 0) {
        return;
    }
    unsigned int base = *(uintptr_t *)((uintptr_t)pointer - 12U);
    free((void *)base);
}

void *__kr32_new_array(int element_size, int count, int alignment) {
    if (element_size <= 0 || count < 0) {
        return 0;
    }
    unsigned int total = (uintptr_t)element_size * (uintptr_t)count;
    if (count != 0 && total / (uintptr_t)count != (uintptr_t)element_size) {
        return 0;
    }
    void *pointer = __kr32_new_alloc((int)total, alignment);
    if (pointer != 0) {
        *(uintptr_t *)((uintptr_t)pointer - 8U) = (uintptr_t)count;
    }
    return pointer;
}

void *calloc(int count, int size) {
    if (count <= 0 || size <= 0) {
        return 0;
    }
    unsigned int total = (uintptr_t)count * (uintptr_t)size;
    if (count != 0 && total / (uintptr_t)count != (uintptr_t)size) {
        return 0;
    }
    void *pointer = malloc((int)total);
    if (pointer != 0) {
        memset(pointer, 0, (int)total);
    }
    return pointer;
}

void *realloc(void *pointer, int size) {
    if (pointer == 0) {
        return malloc(size);
    }
    if (size <= 0) {
        free(pointer);
        return 0;
    }
    struct heap_block *block =
        (struct heap_block *)((uintptr_t)pointer - sizeof(struct heap_block));
    unsigned int old_size = block->size - sizeof(struct heap_block) - 4;
    if (old_size >= (uintptr_t)size) {
        return pointer;
    }
    void *replacement = malloc(size);
    if (replacement == 0) {
        return 0;
    }
    memcpy(replacement, pointer, (int)old_size);
    free(pointer);
    return replacement;
}

static int stream_flush(struct kr32_file *stream);

static int stream_raw_read(struct kr32_file *stream,
                           unsigned char *buffer,
                           int count) {
    if (count <= 0) {
        return 0;
    }
    if (stream->descriptor == 0) {
        int index = 0;
        while (index < count) {
            const int value = getchar();
            if (value < 0) {
                break;
            }
            buffer[index] = (unsigned char)value;
            index = index + 1;
        }
        return index;
    }
    return read(stream->descriptor, buffer, count);
}

static int stream_read_byte(struct kr32_file *stream) {
    if (stream == 0 || (stream->flags & KR32_FILE_READ) == 0) {
        errno = EBADF;
        return EOF;
    }
    if (stream->ungot >= 0) {
        const int value = stream->ungot;
        stream->ungot = -1;
        return value;
    }
    if (stream->buffer_length > 0) {
        if (stream_flush(stream) != 0) {
            return EOF;
        }
    }
    if (stream->memory_mode != 0) {
        if (stream->memory == 0) {
            stream->flags = stream->flags | KR32_FILE_EOF;
            return EOF;
        }
        const char *memory = stream->memory;
        const char value = memory[stream->memory_position];
        if (value == '\0') {
            stream->flags = stream->flags | KR32_FILE_EOF;
            return EOF;
        }
        stream->memory_position = stream->memory_position + 1;
        return (unsigned char)value;
    }
    if (stream->input_capacity > 0 && stream->input_buffer != 0) {
        if (stream->input_begin >= stream->input_end) {
            unsigned char *input = stream->input_buffer;
            const int filled = stream_raw_read(
                stream, input, stream->input_capacity);
            if (filled < 0) {
                stream->flags = stream->flags | KR32_FILE_ERROR;
                errno = EIO;
                return EOF;
            }
            if (filled == 0) {
                stream->flags = stream->flags | KR32_FILE_EOF;
                return EOF;
            }
            stream->input_begin = 0;
            stream->input_end = filled;
        }
        unsigned char *input = stream->input_buffer;
        const int value = input[stream->input_begin];
        stream->input_begin = stream->input_begin + 1;
        return value;
    }
    unsigned char byte = 0;
    const int count = stream_raw_read(stream, &byte, 1);
    if (count == 0) {
        stream->flags = stream->flags | KR32_FILE_EOF;
        return EOF;
    }
    if (count < 0) {
        stream->flags = stream->flags | KR32_FILE_ERROR;
        errno = EIO;
        return EOF;
    }
    return (int)byte;
}

static int stream_write_bytes(struct kr32_file *stream, const void *buffer, int count);

static int stream_raw_write(struct kr32_file *stream, const void *buffer, int count) {
    if (count <= 0) {
        return 0;
    }
    if (stream->descriptor == 1 || stream->descriptor == 2) {
        const unsigned char *bytes = (const unsigned char *)buffer;
        int index = 0;
        while (index < count) {
            if (putchar((int)bytes[index]) < 0) {
                return -1;
            }
            index = index + 1;
        }
        return count;
    }
    return write(stream->descriptor, buffer, count);
}

static int stream_flush(struct kr32_file *stream) {
    if (stream == 0 || stream->buffer_mode == _IONBF || stream->buffer_length == 0) {
        return 0;
    }
    unsigned char *buffer = stream->buffer;
    int total = 0;
    while (total < stream->buffer_length) {
        const int written = stream_raw_write(
            stream, buffer + total, stream->buffer_length - total);
        if (written <= 0) {
            stream->flags = stream->flags | KR32_FILE_ERROR;
            errno = EIO;
            return EOF;
        }
        total = total + written;
    }
    stream->buffer_length = 0;
    return 0;
}

static int stream_write_byte(struct kr32_file *stream, int character) {
    if (stream == 0 || (stream->flags & KR32_FILE_WRITE) == 0) {
        errno = EBADF;
        return EOF;
    }
    const unsigned char byte = (unsigned char)character;
    return stream_write_bytes(stream, &byte, 1) == 1 ? (int)byte : EOF;
}

static int stream_write_bytes(struct kr32_file *stream, const void *buffer, int count) {
    if (stream == 0 || (stream->flags & KR32_FILE_WRITE) == 0 || count <= 0) {
        if (stream == 0 || (stream->flags & KR32_FILE_WRITE) == 0) {
            errno = EBADF;
        }
        return 0;
    }
    const unsigned char *bytes = (const unsigned char *)buffer;
    if (stream->buffer_mode == _IONBF || stream->buffer == 0 ||
        stream->buffer_capacity <= 0) {
        const int written = stream_raw_write(stream, buffer, count);
        if (written < 0) {
            stream->flags = stream->flags | KR32_FILE_ERROR;
            errno = EIO;
            return 0;
        }
        return written;
    }
    int index = 0;
    while (index < count) {
        if (stream->buffer_length >= stream->buffer_capacity) {
            if (stream_flush(stream) != 0) {
                return index;
            }
        }
        const unsigned char byte = bytes[index];
        unsigned char *destination = stream->buffer;
        destination[stream->buffer_length] = byte;
        stream->buffer_length = stream->buffer_length + 1;
        index = index + 1;
        if (stream->buffer_mode == _IOLBF && byte == '\n') {
            if (stream_flush(stream) != 0) {
                return index;
            }
        }
    }
    return index;
}

struct kr32_file *fopen(const char *path, const char *mode) {
    if (path == 0 || mode == 0 || mode[0] == '\0') {
        errno = EINVAL;
        return 0;
    }
    int readable = 0;
    int writable = 0;
    int create = 0;
    int truncate = 0;
    int append = 0;
    if (mode[0] == 'r') {
        readable = 1;
    } else if (mode[0] == 'w') {
        writable = 1;
        create = 1;
        truncate = 1;
    } else if (mode[0] == 'a') {
        writable = 1;
        create = 1;
        append = 1;
    } else {
        errno = EINVAL;
        return 0;
    }
    int index = 0;
    while (mode[index] != '\0') {
        if (mode[index] == '+') {
            readable = 1;
            writable = 1;
        }
        index = index + 1;
    }
    int flags = 0;
    if (readable && writable) {
        flags = 2;
    } else if (writable) {
        flags = 1;
    }
    if (create) flags = flags | 4;
    if (truncate) flags = flags | 8;
    if (append) flags = flags | 16;
    const int descriptor = open(path, flags);
    if (descriptor < 0) {
        errno = EIO;
        return 0;
    }
    struct kr32_file *stream = (struct kr32_file *)malloc(sizeof(struct kr32_file));
    if (stream == 0) {
        close(descriptor);
        errno = ENOMEM;
        return 0;
    }
    stream->descriptor = descriptor;
    stream->flags = 0;
    if (readable) stream->flags = stream->flags | KR32_FILE_READ;
    if (writable) stream->flags = stream->flags | KR32_FILE_WRITE;
    stream->ungot = -1;
    stream->owns_descriptor = 1;
    stream->memory = 0;
    stream->memory_position = 0;
    stream->memory_mode = 0;
    stream->buffer = 0;
    stream->buffer_capacity = 0;
    stream->buffer_length = 0;
    stream->buffer_mode = _IONBF;
    stream->buffer_owned = 0;
    stream->input_buffer = 0;
    stream->input_capacity = 0;
    stream->input_begin = 0;
    stream->input_end = 0;
    stream->input_owned = 0;
    return stream;
}

struct kr32_file *fdopen(int descriptor, const char *mode) {
    if (descriptor < 0 || mode == 0) {
        errno = EINVAL;
        return 0;
    }
    int readable = 0;
    int writable = 0;
    if (mode[0] == 'r') {
        readable = 1;
    } else if (mode[0] == 'w' || mode[0] == 'a') {
        writable = 1;
    } else {
        errno = EINVAL;
        return 0;
    }
    int index = 0;
    while (mode[index] != '\0') {
        if (mode[index] == '+') {
            readable = 1;
            writable = 1;
        }
        index = index + 1;
    }
    struct kr32_file *stream = (struct kr32_file *)malloc(sizeof(struct kr32_file));
    if (stream == 0) {
        errno = ENOMEM;
        return 0;
    }
    stream->descriptor = descriptor;
    stream->flags = 0;
    if (readable) stream->flags = stream->flags | KR32_FILE_READ;
    if (writable) stream->flags = stream->flags | KR32_FILE_WRITE;
    stream->ungot = -1;
    stream->owns_descriptor = 1;
    stream->memory = 0;
    stream->memory_position = 0;
    stream->memory_mode = 0;
    stream->buffer = 0;
    stream->buffer_capacity = 0;
    stream->buffer_length = 0;
    stream->buffer_mode = _IONBF;
    stream->buffer_owned = 0;
    stream->input_buffer = 0;
    stream->input_capacity = 0;
    stream->input_begin = 0;
    stream->input_end = 0;
    stream->input_owned = 0;
    return stream;
}

struct kr32_file *freopen(const char *path, const char *mode,
                          struct kr32_file *stream) {
    if (stream == 0) {
        return fopen(path, mode);
    }
    if (stream->descriptor < 3) {
        errno = EINVAL;
        return 0;
    }
    struct kr32_file *replacement = fopen(path, mode);
    if (replacement == 0) {
        return 0;
    }
    (void)stream_flush(stream);
    if (stream->buffer_owned) {
        free(stream->buffer);
    }
    if (stream->input_owned) {
        free(stream->input_buffer);
    }
    if (stream->owns_descriptor) {
        (void)close(stream->descriptor);
    }
    stream->descriptor = replacement->descriptor;
    stream->flags = replacement->flags;
    stream->ungot = replacement->ungot;
    stream->owns_descriptor = replacement->owns_descriptor;
    stream->memory = replacement->memory;
    stream->memory_position = replacement->memory_position;
    stream->memory_mode = replacement->memory_mode;
    stream->buffer = replacement->buffer;
    stream->buffer_capacity = replacement->buffer_capacity;
    stream->buffer_length = replacement->buffer_length;
    stream->buffer_mode = replacement->buffer_mode;
    stream->buffer_owned = replacement->buffer_owned;
    stream->input_buffer = replacement->input_buffer;
    stream->input_capacity = replacement->input_capacity;
    stream->input_begin = replacement->input_begin;
    stream->input_end = replacement->input_end;
    stream->input_owned = replacement->input_owned;
    free(replacement);
    return stream;
}

int fclose(struct kr32_file *stream) {
    if (stream == 0) {
        errno = EINVAL;
        return EOF;
    }
    int result = stream_flush(stream);
    if (stream->descriptor < 3) {
        return result == 0 ? 0 : EOF;
    }
    if (stream->owns_descriptor) {
        const int closed = close(stream->descriptor);
        if (closed < 0 && result == 0) {
            result = closed;
        }
    }
    if (stream->buffer_owned) {
        free(stream->buffer);
    }
    if (stream->input_owned) {
        free(stream->input_buffer);
    }
    free(stream);
    if (result < 0) {
        errno = EIO;
        return EOF;
    }
    return 0;
}

int fflush(struct kr32_file *stream) {
    if (stream == 0) {
        (void)stream_flush(stdout);
        (void)stream_flush(stderr);
        return 0;
    }
    return stream_flush(stream);
}

int fgetc(struct kr32_file *stream) {
    return stream_read_byte(stream);
}

int ungetc(int character, struct kr32_file *stream) {
    if (stream == 0 || character == EOF || stream->ungot != -1) {
        return EOF;
    }
    stream->ungot = character & 0xFF;
    stream->flags = stream->flags & ~KR32_FILE_EOF;
    return stream->ungot;
}

int fputc(int character, struct kr32_file *stream) {
    return stream_write_byte(stream, character);
}

int fputs(const char *text, struct kr32_file *stream) {
    if (text == 0 || stream == 0) {
        return EOF;
    }
    int index = 0;
    while (text[index] != '\0') {
        if (stream_write_byte(stream, (int)(unsigned char)text[index]) == EOF) {
            return EOF;
        }
        index = index + 1;
    }
    return 0;
}

int puts(const char *text) {
    if (fputs(text, stdout) == EOF) {
        return EOF;
    }
    if (fputc('\n', stdout) == EOF) {
        return EOF;
    }
    return 0;
}

char *fgets(char *buffer, int size, struct kr32_file *stream) {
    if (buffer == 0 || size <= 0 || stream == 0) {
        return 0;
    }
    int index = 0;
    while (index < size - 1) {
        const int value = fgetc(stream);
        if (value == EOF) {
            if (index == 0) {
                return 0;
            }
            break;
        }
        buffer[index] = (char)value;
        index = index + 1;
        if (value == '\n') {
            break;
        }
    }
    buffer[index] = '\0';
    return buffer;
}

static int is_space_character(int character) {
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

static void scanf_skip_space(struct kr32_file *stream) {
    int character = fgetc(stream);
    while (character != EOF && is_space_character(character)) {
        character = fgetc(stream);
    }
    if (character != EOF) {
        ungetc(character, stream);
    }
}

static int scanf_integer(struct kr32_file *stream, unsigned int *value, int base) {
    scanf_skip_space(stream);
    int character = fgetc(stream);
    int negative = 0;
    if (character == '+' || character == '-') {
        negative = character == '-';
        character = fgetc(stream);
    }
    if (base == 0) {
        base = 10;
        if (character == '0') {
            base = 8;
            character = fgetc(stream);
            if (character == 'x' || character == 'X') {
                base = 16;
                character = fgetc(stream);
            }
        }
    } else if (base == 16 && character == '0') {
        const int next = fgetc(stream);
        if (next == 'x' || next == 'X') {
            character = fgetc(stream);
        } else {
            ungetc(next, stream);
        }
    }
    int digits = 0;
    unsigned int result = 0;
    while (character != EOF) {
        int digit = -1;
        if (character >= '0' && character <= '9') {
            digit = character - '0';
        } else if (character >= 'a' && character <= 'f') {
            digit = character - 'a' + 10;
        } else if (character >= 'A' && character <= 'F') {
            digit = character - 'A' + 10;
        }
        if (digit < 0 || digit >= base) {
            break;
        }
        result = result * (uintptr_t)base + (uintptr_t)digit;
        digits = digits + 1;
        character = fgetc(stream);
    }
    if (character != EOF) {
        ungetc(character, stream);
    }
    if (digits == 0) {
        return 0;
    }
    *value = negative ? (uintptr_t)(0 - result) : result;
    return 1;
}

static int scanf_double(struct kr32_file *stream, double *value) {
    scanf_skip_space(stream);
    int character = fgetc(stream);
    int negative = 0;
    if (character == '+' || character == '-') {
        negative = character == '-';
        character = fgetc(stream);
    }
    int digits = 0;
    double result = 0.0;
    while (character >= '0' && character <= '9') {
        result = result * 10.0 + (double)(character - '0');
        digits = digits + 1;
        character = fgetc(stream);
    }
    if (character == '.') {
        character = fgetc(stream);
        double place = 0.1;
        while (character >= '0' && character <= '9') {
            result = result + (double)(character - '0') * place;
            place = place * 0.1;
            digits = digits + 1;
            character = fgetc(stream);
        }
    }
    if (digits == 0) {
        if (character != EOF) {
            ungetc(character, stream);
        }
        return 0;
    }
    if (character == 'e' || character == 'E') {
        character = fgetc(stream);
        int exponent_negative = 0;
        if (character == '+' || character == '-') {
            exponent_negative = character == '-';
            character = fgetc(stream);
        }
        int exponent = 0;
        int exponent_digits = 0;
        while (character >= '0' && character <= '9') {
            exponent = exponent * 10 + (character - '0');
            exponent_digits = exponent_digits + 1;
            character = fgetc(stream);
        }
        if (exponent_digits == 0) {
            if (character != EOF) {
                ungetc(character, stream);
            }
        } else {
            int power = 0;
            while (power < exponent) {
                result = exponent_negative ? result / 10.0 : result * 10.0;
                power = power + 1;
            }
        }
    }
    if (character != EOF) {
        ungetc(character, stream);
    }
    *value = negative ? 0.0 - result : result;
    return 1;
}

static int stream_position(struct kr32_file *stream) {
    if (stream == 0) {
        return -1;
    }
    if (stream->memory_mode != 0) {
        return stream->memory_position;
    }
    if (stream->descriptor < 3) {
        return -1;
    }
    if (stream_flush(stream) != 0) {
        return -1;
    }
    return seek(stream->descriptor, 0, 1);
}

int feof(struct kr32_file *stream);

int vfscanf(struct kr32_file *stream, const char *format, va_list arguments) {
    if (stream == 0 || format == 0) {
        errno = EINVAL;
        return EOF;
    }
    int assigned = 0;
    const int start_position = stream_position(stream);
    const char *cursor = format;
    while (*cursor != '\0') {
        if (is_space_character((int)*cursor)) {
            scanf_skip_space(stream);
            cursor = cursor + 1;
            continue;
        }
        if (*cursor != '%') {
            const int character = fgetc(stream);
            if (character != (int)(unsigned char)*cursor) {
                if (character != EOF) {
                    ungetc(character, stream);
                }
                break;
            }
            cursor = cursor + 1;
            continue;
        }
        cursor = cursor + 1;
        if (*cursor == '%') {
            const int character = fgetc(stream);
            if (character != '%') {
                if (character != EOF) {
                    ungetc(character, stream);
                }
                break;
            }
            cursor = cursor + 1;
            continue;
        }
        int suppress = 0;
        if (*cursor == '*') {
            suppress = 1;
            cursor = cursor + 1;
        }
        int width = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            width = width * 10 + (*cursor - '0');
            cursor = cursor + 1;
        }
        while (*cursor == 'h' || *cursor == 'l' || *cursor == 'j' ||
               *cursor == 'z' || *cursor == 't' || *cursor == 'L') {
            cursor = cursor + 1;
        }
        const char spec = *cursor;
        if (spec == '\0') {
            break;
        }
        cursor = cursor + 1;
        if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' ||
            spec == 'X' || spec == 'o') {
            unsigned int value = 0;
            const int base = (spec == 'x' || spec == 'X')
                                 ? 16
                                 : (spec == 'o' ? 8 : (spec == 'i' ? 0 : 10));
            if (!scanf_integer(stream, &value, base)) {
                break;
            }
            if (!suppress) {
                unsigned int *target = va_arg(arguments, unsigned int *);
                *target = value;
                assigned = assigned + 1;
            }
        } else if (spec == 'n') {
            if (!suppress) {
                int *target = va_arg(arguments, int *);
                int current = stream_position(stream);
                if (current > 0 && stream->ungot >= 0) {
                    current = current - 1;
                }
                *target = current >= 0 && start_position >= 0
                              ? current - start_position
                              : 0;
            }
        } else if (spec == 'p') {
            unsigned int value = 0;
            if (!scanf_integer(stream, &value, 16)) {
                break;
            }
            if (!suppress) {
                void **target = va_arg(arguments, void **);
                *target = (void *)value;
                assigned = assigned + 1;
            }
        } else if (spec == 'f' || spec == 'e' || spec == 'g') {
            double value = 0.0;
            if (!scanf_double(stream, &value)) {
                break;
            }
            if (!suppress) {
                double *target = va_arg(arguments, double *);
                *target = value;
                assigned = assigned + 1;
            }
        } else if (spec == '[') {
            int negate = 0;
            if (*cursor == '^') {
                negate = 1;
                cursor = cursor + 1;
            }
            unsigned char set[256];
            int set_index = 0;
            while (set_index < 256) {
                set[set_index] = 0;
                set_index = set_index + 1;
            }
            int any = 0;
            while (*cursor != '\0' && *cursor != ']') {
                unsigned char first = (unsigned char)*cursor;
                cursor = cursor + 1;
                unsigned char last = first;
                if (*cursor == '-' && cursor[1] != ']' && cursor[1] != '\0') {
                    cursor = cursor + 1;
                    last = (unsigned char)*cursor;
                    cursor = cursor + 1;
                }
                int code = (int)first;
                while (code <= (int)last) {
                    set[code] = 1;
                    code = code + 1;
                }
                any = 1;
            }
            if (!any) {
                break;
            }
            if (*cursor == ']') {
                cursor = cursor + 1;
            }
            char *target = 0;
            char *destination = 0;
            if (!suppress) {
                target = va_arg(arguments, char *);
                destination = target;
            }
            int index = 0;
            int character = fgetc(stream);
            while (character != EOF) {
                const int matched = set[(unsigned char)character] != 0;
                if ((negate ? !matched : matched) == 0) {
                    break;
                }
                if (width == 0 || index < width - 1) {
                    if (!suppress) {
                        *destination = (char)character;
                        destination = destination + 1;
                    }
                    index = index + 1;
                }
                character = fgetc(stream);
            }
            if (character != EOF) {
                ungetc(character, stream);
            }
            if (!suppress) {
                *destination = '\0';
            }
            if (index == 0) {
                break;
            }
            if (!suppress) {
                assigned = assigned + 1;
            }
        } else if (spec == 's') {
            int character = fgetc(stream);
            while (character != EOF && is_space_character(character)) {
                character = fgetc(stream);
            }
            if (suppress) {
                while (character != EOF && !is_space_character(character)) {
                    character = fgetc(stream);
                }
                if (character != EOF) {
                    ungetc(character, stream);
                }
            } else {
                char *target = va_arg(arguments, char *);
                char *destination = target;
                int index = 0;
                while (character != EOF && !is_space_character(character)) {
                    if (width == 0 || index < width - 1) {
                        *destination = (char)character;
                        destination = destination + 1;
                        index = index + 1;
                    }
                    character = fgetc(stream);
                }
                if (character != EOF) {
                    ungetc(character, stream);
                }
                *destination = '\0';
                if (index == 0) {
                    break;
                }
                assigned = assigned + 1;
            }
        } else if (spec == 'c') {
            if (suppress) {
                int count = width == 0 ? 1 : width;
                while (count > 0) {
                    if (fgetc(stream) == EOF) {
                        break;
                    }
                    count = count - 1;
                }
            } else {
                char *target = va_arg(arguments, char *);
                int count = width == 0 ? 1 : width;
                int index = 0;
                while (index < count) {
                    const int character = fgetc(stream);
                    if (character == EOF) {
                        break;
                    }
                    target[index] = (char)character;
                    index = index + 1;
                }
                if (index == 0) {
                    break;
                }
                assigned = assigned + 1;
            }
        } else {
            break;
        }
    }
    if (assigned == 0) {
        return feof(stream) ? EOF : 0;
    }    return assigned;
}

int fscanf(struct kr32_file *stream, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vfscanf(stream, format, arguments);
    va_end(arguments);
    return result;
}

int vsscanf(const char *text, const char *format, va_list arguments) {
    if (text == 0 || format == 0) {
        errno = EINVAL;
        return EOF;
    }
    struct kr32_file source;
    source.descriptor = -1;
    source.flags = KR32_FILE_READ;
    source.ungot = -1;
    source.owns_descriptor = 0;
    source.memory = text;
    source.memory_position = 0;
    source.memory_mode = 1;
    return vfscanf(&source, format, arguments);
}

int sscanf(const char *text, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vsscanf(text, format, arguments);
    va_end(arguments);
    return result;
}

int scanf(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vfscanf(stdin, format, arguments);
    va_end(arguments);
    return result;
}

int getdelim(char **line, int *capacity, int delimiter, struct kr32_file *stream) {
    if (line == 0 || capacity == 0 || stream == 0) {
        errno = EINVAL;
        return -1;
    }
    if (*line == 0 || *capacity <= 0) {
        *capacity = 128;
        *line = (char *)malloc(*capacity);
        if (*line == 0) {
            errno = ENOMEM;
            return -1;
        }
    }
    int length = 0;
    char *destination = *line;
    while (1) {
        const int character = fgetc(stream);
        if (character == EOF) {
            if (length == 0) {
                return -1;
            }
            break;
        }
        if (length + 1 >= *capacity) {
            const int next_capacity = *capacity * 2;
            char *replacement = (char *)realloc(*line, next_capacity);
            if (replacement == 0) {
                errno = ENOMEM;
                return -1;
            }
            *line = replacement;
            destination = replacement;
            *capacity = next_capacity;
        }
        destination[length] = (char)character;
        length = length + 1;
        if (character == delimiter) {
            break;
        }
    }
    destination[length] = '\0';
    return length;
}

int getline(char **line, int *capacity, struct kr32_file *stream) {
    return getdelim(line, capacity, '\n', stream);
}

int fread(void *buffer, int size, int count, struct kr32_file *stream) {
    if (buffer == 0 || size <= 0 || count <= 0 || stream == 0) {
        if (stream == 0 || buffer == 0) {
            errno = EINVAL;
        }
        return 0;
    }
    if (stream_flush(stream) != 0) {
        return 0;
    }
    const int requested = size * count;
    int total = 0;
    unsigned char *bytes = (unsigned char *)buffer;
    if (stream->descriptor == 0 || stream->input_capacity > 0) {
        while (total < requested) {
            const int value = fgetc(stream);
            if (value == EOF) {
                break;
            }
            bytes[total] = (unsigned char)value;
            total = total + 1;
        }
    } else {
        const int result = read(stream->descriptor, buffer, requested);
        if (result < 0) {
            stream->flags = stream->flags | KR32_FILE_ERROR;
            errno = result == -4 ? EBADF : EIO;
            return 0;
        }
        if (result == 0) {
            stream->flags = stream->flags | KR32_FILE_EOF;
        }
        total = result;
    }
    return total / size;
}

int fwrite(const void *buffer, int size, int count, struct kr32_file *stream) {
    if (buffer == 0 || size <= 0 || count <= 0 || stream == 0) {
        if (stream == 0 || buffer == 0) {
            errno = EINVAL;
        }
        return 0;
    }
    const int requested = size * count;
    const int written = stream_write_bytes(stream, buffer, requested);
    return written / size;
}

int fseek(struct kr32_file *stream, int offset, int whence) {
    if (stream == 0 || stream->descriptor < 3) {
        errno = EINVAL;
        return EOF;
    }
    if (stream_flush(stream) != 0) {
        return EOF;
    }
    stream->input_begin = 0;
    stream->input_end = 0;
    const int result = seek(stream->descriptor, offset, whence);
    stream->flags = stream->flags & ~KR32_FILE_EOF;
    if (result < 0) {
        stream->flags = stream->flags | KR32_FILE_ERROR;
        errno = result == -4 ? EBADF : EIO;
        return EOF;
    }
    return 0;
}

int ftell(struct kr32_file *stream) {
    if (stream == 0 || stream->descriptor < 3) {
        errno = EINVAL;
        return EOF;
    }
    if (stream_flush(stream) != 0) {
        return EOF;
    }
    const int result = seek(stream->descriptor, 0, 1);
    if (result < 0) {
        errno = result == -4 ? EBADF : EIO;
    }
    return result;
}

void rewind(struct kr32_file *stream) {
    if (stream != 0) {
        (void)fseek(stream, 0, 0);
        clearerr(stream);
    }
}

int fgetpos(struct kr32_file *stream, int *position) {
    if (position == 0) {
        errno = EINVAL;
        return -1;
    }
    const int current = ftell(stream);
    if (current < 0) {
        return -1;
    }
    *position = current;
    return 0;
}

int fsetpos(struct kr32_file *stream, const int *position) {
    if (position == 0) {
        errno = EINVAL;
        return -1;
    }
    return fseek(stream, *position, 0);
}

int setvbuf(struct kr32_file *stream, char *buffer, int mode, int size) {
    if (stream == 0 || mode < _IOFBF || mode > _IONBF) {
        errno = EINVAL;
        return -1;
    }
    if (stream_flush(stream) != 0) {
        return -1;
    }
    if (stream->buffer_owned) {
        free(stream->buffer);
        stream->buffer = 0;
        stream->buffer_owned = 0;
    }
    if (stream->input_owned) {
        free(stream->input_buffer);
        stream->input_buffer = 0;
        stream->input_owned = 0;
    }
    stream->buffer_capacity = 0;
    stream->buffer_length = 0;
    stream->buffer_mode = mode;
    stream->input_capacity = 0;
    stream->input_begin = 0;
    stream->input_end = 0;
    if (mode == _IONBF) {
        stream->buffer = 0;
        return 0;
    }
    if (size <= 0) {
        size = BUFSIZ;
    }
    if (buffer == 0) {
        buffer = (char *)malloc(size);
        if (buffer == 0) {
            errno = ENOMEM;
            return -1;
        }
        stream->buffer_owned = 1;
    }
    unsigned char *input = (unsigned char *)malloc(size);
    if (input == 0) {
        if (stream->buffer_owned) {
            free(buffer);
            stream->buffer = 0;
            stream->buffer_owned = 0;
        }
        errno = ENOMEM;
        return -1;
    }
    stream->input_buffer = input;
    stream->input_capacity = size;
    stream->input_owned = 1;
    stream->buffer = (unsigned char *)buffer;
    stream->buffer_capacity = size;
    return 0;
}

void setbuf(struct kr32_file *stream, char *buffer) {
    (void)setvbuf(stream, buffer, buffer == 0 ? _IONBF : _IOFBF, BUFSIZ);
}

int feof(struct kr32_file *stream) {
    return stream != 0 && (stream->flags & KR32_FILE_EOF) != 0;
}

int ferror(struct kr32_file *stream) {
    return stream != 0 && (stream->flags & KR32_FILE_ERROR) != 0;
}

void clearerr(struct kr32_file *stream) {
    if (stream != 0) {
        stream->flags = stream->flags & ~(KR32_FILE_EOF | KR32_FILE_ERROR);
    }
}

int fileno(struct kr32_file *stream) {
    return stream == 0 ? -1 : stream->descriptor;
}

int isatty(int descriptor) {
    return descriptor >= 0 && descriptor <= 2;
}

int fsync(int descriptor) {
    if (descriptor < 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

int fdatasync(int descriptor) {
    return fsync(descriptor);
}

char *getcwd(char *buffer, int size) {
    if (size <= 0) {
        errno = EINVAL;
        return 0;
    }
    int allocated = 0;
    if (buffer == 0) {
        buffer = (char *)malloc(size);
        if (buffer == 0) {
            errno = ENOMEM;
            return 0;
        }
        allocated = 1;
    }
    const int result = kr32_getcwd(buffer, size);
    if (result < 0) {
        if (allocated) {
            free(buffer);
        }
        errno = result == -4 ? EINVAL : EIO;
        return 0;
    }
    return buffer;
}

int chdir(const char *path) {
    if (path == 0) {
        errno = EINVAL;
        return -1;
    }
    const int result = kr32_chdir(path);
    if (result < 0) {
        errno = result == -4 ? ENOENT : EIO;
        return -1;
    }
    return 0;
}

int access(const char *path, int mode) {
    if (path == 0) {
        errno = EINVAL;
        return -1;
    }
    struct stat info;
    if (stat(path, &info) != 0) {
        errno = ENOENT;
        return -1;
    }
    if (mode == 0) {
        return 0;
    }
    if ((mode & 2) != 0) {
        return 0;
    }
    return 0;
}

int remove(const char *path) {
    if (path == 0) {
        errno = EINVAL;
        return -1;
    }
    const int result = unlink(path);
    if (result < 0) {
        errno = result == -4 ? EINVAL : EIO;
        return -1;
    }
    return 0;
}

int fstat(int descriptor, struct stat *status) {
    if (status == 0) {
        errno = EINVAL;
        return -1;
    }
    const int current = seek(descriptor, 0, 1);
    if (current < 0) {
        errno = EBADF;
        return -1;
    }
    const int end = seek(descriptor, 0, 2);
    if (end < 0) {
        errno = EBADF;
        return -1;
    }
    (void)seek(descriptor, current, 0);
    status->st_size = (uintptr_t)end;
    status->st_mode = 0x8000;
    status->st_mtime = 0;
    status->st_reserved = 0;
    return 0;
}

const char *strerror(int code) {
    switch (code) {
    case 0: return "no error";
    case 1: return "operation not permitted";
    case 2: return "no such file or directory";
    case 5: return "input/output error";
    case 9: return "bad file descriptor";
    case 12: return "out of memory";
    case 13: return "permission denied";
    case 17: return "file exists";
    case 22: return "invalid argument";
    case 28: return "no space left on device";
    case 38: return "function not implemented";
    default: return "unknown error";
    }
}

void perror(const char *prefix) {
    if (prefix != 0 && prefix[0] != '\0') {
        fputs(prefix, stderr);
        fputs(": ", stderr);
    }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}

static int heap_free_list_contains(struct heap_block *target) {
    struct heap_block *current = heap_free_list;
    int guard = 0;
    while (current != 0 && guard < 1000000) {
        if (current == target) {
            return 1;
        }
        current = (struct heap_block *)current->next;
        guard = guard + 1;
    }
    return 0;
}

/*
 * Validate the physical block chain and the address-ordered free list, then
 * recompute the exported counters. Returns 0 on success, or a small nonzero
 * diagnostic code. This is a diagnostic, not a repair of corrupt links.
 */
int _heapwalk(void) {
    if (!heap_ready) {
        heap_init();
        if (!heap_ready) {
            return 1;
        }
    }
    unsigned int allocated = 0;
    unsigned int free_count = 0;
    unsigned int address = heap_begin;
    int guard = 0;
    while (address < heap_end_address) {
        if (heap_end_address - address < sizeof(struct heap_block)) {
            return 2;
        }
        struct heap_block *block = (struct heap_block *)address;
        if (block->size < sizeof(struct heap_block) + 12 ||
            (block->size & 7) != 0 || block->size > heap_end_address - address) {
            return 2;
        }
        if (heap_footer(block) != block->size) {
            return 3;
        }
        if (block->is_free) {
            free_count = free_count + 1;
            if (!heap_free_list_contains(block)) {
                return 4;
            }
        } else {
            allocated = allocated + 1;
        }
        address = address + block->size;
        guard = guard + 1;
        if (guard > 1000000) {
            return 2;
        }
    }
    if (address != heap_end_address) {
        return 2;
    }

    struct heap_block *current = heap_free_list;
    unsigned int previous = 0;
    unsigned int listed = 0;
    guard = 0;
    while (current != 0) {
        const unsigned int block_address = (uintptr_t)current;
        if (block_address < heap_begin ||
            block_address + sizeof(struct heap_block) > heap_end_address) {
            return 4;
        }
        if (!current->is_free) {
            return 4;
        }
        if (previous != 0 && block_address <= previous) {
            return 4;
        }
        if (current->prev != previous) {
            return 4;
        }
        previous = block_address;
        listed = listed + 1;
        current = (struct heap_block *)current->next;
        guard = guard + 1;
        if (guard > 1000000) {
            return 4;
        }
    }
    if (listed != free_count) {
        return 5;
    }

    /* The walk is authoritative; resynchronize stale debug counters. */
    heap_allocated_blocks = allocated;
    heap_free_blocks = free_count;
    return 0;
}

struct string_sink {
    char *buffer;
    int capacity;
    int length;
};

static int format_target = 0; /* 0 console, 1 file, 2 string */
static struct kr32_file *format_stream = 0;
static struct string_sink *format_sink = 0;

static void emit_character(int character) {
    if (format_target == 1 && format_stream != 0) {
        fputc(character, format_stream);
    } else if (format_target == 2 && format_sink != 0) {
        if (format_sink->capacity < 0 ||
            format_sink->length < format_sink->capacity - 1) {
            char *destination = format_sink->buffer;
            if (destination != 0) {
                destination[format_sink->length] = (char)character;
            }
        }
        format_sink->length = format_sink->length + 1;
    } else {
        putchar(character);
    }
}

static int out_pad(int count, char c) {
    int written = 0;
    while (written < count) {
        emit_character((int)c);
        written = written + 1;
    }
    return written;
}

static int out_field(const char *text, int length, int width, int flags) {
    int left = flags & 1;
    int zero = (flags >> 1) & 1;
    int prefix = flags >> 8;
    int padding = width - length;
    int written = 0;
    int index = 0;
    if (padding < 0) {
        padding = 0;
    }
    if (left) {
        while (index < length) {
            emit_character((int)text[index]);
            written = written + 1;
            index = index + 1;
        }
        written = written + out_pad(padding, ' ');
        return written;
    }
    if (zero) {
        while (index < prefix && index < length) {
            emit_character((int)text[index]);
            written = written + 1;
            index = index + 1;
        }
        written = written + out_pad(padding, '0');
        while (index < length) {
            emit_character((int)text[index]);
            written = written + 1;
            index = index + 1;
        }
        return written;
    }
    written = written + out_pad(padding, ' ');
    while (index < length) {
        emit_character((int)text[index]);
        written = written + 1;
        index = index + 1;
    }
    return written;
}

static int format_integer(char *buffer, unsigned int value, int base, int flags) {
    int negative = flags & 1;
    int plus = (flags >> 1) & 1;
    int space = (flags >> 2) & 1;
    int alternate = (flags >> 3) & 1;
    int upper = (flags >> 4) & 1;
    char digits[32];
    int digit_count = 0;
    int position = 0;
    if (negative) {
        buffer[position] = '-';
        position = position + 1;
    } else if (plus) {
        buffer[position] = '+';
        position = position + 1;
    } else if (space) {
        buffer[position] = ' ';
        position = position + 1;
    }
    if (alternate) {
        if (base == 16) {
            buffer[position] = '0';
            position = position + 1;
            if (upper) {
                buffer[position] = 'X';
            } else {
                buffer[position] = 'x';
            }
            position = position + 1;
        } else if (base == 8 && value != 0) {
            buffer[position] = '0';
            position = position + 1;
        }
    }
    if (value == 0) {
        digits[digit_count] = '0';
        digit_count = digit_count + 1;
    }
    while (value != 0) {
        int digit = (int)(value % (uintptr_t)base);
        if (digit < 0) {
            digit = 0 - digit;
        }
        if (digit < 10) {
            digits[digit_count] = (char)('0' + digit);
        } else {
            if (upper) {
                digits[digit_count] = (char)('A' + (digit - 10));
            } else {
                digits[digit_count] = (char)('a' + (digit - 10));
            }
        }
        digit_count = digit_count + 1;
        value = value / (uintptr_t)base;
    }
    while (digit_count > 0) {
        digit_count = digit_count - 1;
        buffer[position] = digits[digit_count];
        position = position + 1;
    }
    return position;
}

static int format_integer64(char *buffer, unsigned long long value,
                            int base_flags) {
    int base = (base_flags >> 8) & 255;
    int flags = base_flags & 255;
    int negative = flags & 1;
    int plus = (flags >> 1) & 1;
    int space = (flags >> 2) & 1;
    int alternate = (flags >> 3) & 1;
    int upper = (flags >> 4) & 1;
    char digits[80];
    int digit_count = 0;
    int position = 0;
    if (negative) {
        buffer[position] = '-';
        position = position + 1;
    } else if (plus) {
        buffer[position] = '+';
        position = position + 1;
    } else if (space) {
        buffer[position] = ' ';
        position = position + 1;
    }
    if (alternate) {
        if (base == 16) {
            buffer[position] = '0';
            position = position + 1;
            if (upper) {
                buffer[position] = 'X';
            } else {
                buffer[position] = 'x';
            }
            position = position + 1;
        } else if (base == 8 && value != 0) {
            buffer[position] = '0';
            position = position + 1;
        }
    }
    if (value == 0) {
        digits[digit_count] = '0';
        digit_count = digit_count + 1;
    }
    while (value != 0) {
        int digit = (int)(value % (unsigned long long)base);
        if (digit < 10) {
            digits[digit_count] = (char)('0' + digit);
        } else {
            if (upper) {
                digits[digit_count] = (char)('A' + (digit - 10));
            } else {
                digits[digit_count] = (char)('a' + (digit - 10));
            }
        }
        digit_count = digit_count + 1;
        value = value / (unsigned long long)base;
    }
    while (digit_count > 0) {
        digit_count = digit_count - 1;
        buffer[position] = digits[digit_count];
        position = position + 1;
    }
    return position;
}

static int format_double_fixed(char *buffer, double value, int precision, int flags) {
    int plus = flags & 1;
    int space = (flags >> 1) & 1;
    int alternate = (flags >> 2) & 1;
    int position = 0;
    if (precision < 0) {
        precision = 6;
    }
    if (precision > 9) {
        precision = 9;
    }
    if (value < 0.0) {
        buffer[position] = '-';
        position = position + 1;
        value = 0.0 - value;
    } else if (plus) {
        buffer[position] = '+';
        position = position + 1;
    } else if (space) {
        buffer[position] = ' ';
        position = position + 1;
    }
    double scale = 1.0;
    int index = 0;
    while (index < precision) {
        scale = scale * 10.0;
        index = index + 1;
    }
    double scaled = value * scale + 0.5;
    unsigned int whole = (uintptr_t)(scaled / scale);
    unsigned int fraction = (uintptr_t)(scaled - (double)whole * scale);
    if ((double)fraction >= scale) {
        whole = whole + 1;
        fraction = 0;
    }
    char digits[40];
    int digit_count = 0;
    if (whole == 0) {
        digits[digit_count] = '0';
        digit_count = digit_count + 1;
    }
    while (whole > 0) {
        digits[digit_count] = (char)('0' + (int)(whole % 10));
        digit_count = digit_count + 1;
        whole = whole / 10;
    }
    while (digit_count > 0) {
        digit_count = digit_count - 1;
        buffer[position] = digits[digit_count];
        position = position + 1;
    }
    if (precision > 0 || alternate) {
        buffer[position] = '.';
        position = position + 1;
    }
    if (precision > 0) {
        unsigned int divisor = 1;
        index = 1;
        while (index < precision) {
            divisor = divisor * 10;
            index = index + 1;
        }
        while (divisor > 0) {
            buffer[position] = (char)('0' + (int)((fraction / divisor) % 10));
            position = position + 1;
            divisor = divisor / 10;
        }
    }
    return position;
}

static int format_double_exp(char *buffer, double value, int precision, int flags) {
    int plus = flags & 1;
    int space = (flags >> 1) & 1;
    int alternate = (flags >> 2) & 1;
    int upper = (flags >> 3) & 1;
    int position = 0;
    if (precision < 0) {
        precision = 6;
    }
    if (precision > 9) {
        precision = 9;
    }
    if (value < 0.0) {
        buffer[position] = '-';
        position = position + 1;
        value = 0.0 - value;
    } else if (plus) {
        buffer[position] = '+';
        position = position + 1;
    } else if (space) {
        buffer[position] = ' ';
        position = position + 1;
    }
    int exponent = 0;
    if (value != 0.0) {
        while (value >= 10.0) {
            value = value / 10.0;
            exponent = exponent + 1;
        }
        while (value < 1.0) {
            value = value * 10.0;
            exponent = exponent - 1;
        }
    }
    double scale = 1.0;
    int index = 0;
    while (index < precision) {
        scale = scale * 10.0;
        index = index + 1;
    }
    double scaled = value * scale + 0.5;
    if (scaled >= scale * 10.0) {
        value = value / 10.0;
        exponent = exponent + 1;
        scaled = value * scale + 0.5;
    }
    unsigned int whole = (uintptr_t)(scaled / scale);
    unsigned int fraction = (uintptr_t)(scaled - (double)whole * scale);
    if (whole > 9) {
        whole = 9;
    }
    buffer[position] = (char)('0' + (int)whole);
    position = position + 1;
    if (precision > 0 || alternate) {
        buffer[position] = '.';
        position = position + 1;
    }
    if (precision > 0) {
        unsigned int divisor = 1;
        index = 1;
        while (index < precision) {
            divisor = divisor * 10;
            index = index + 1;
        }
        while (divisor > 0) {
            buffer[position] = (char)('0' + (int)((fraction / divisor) % 10));
            position = position + 1;
            divisor = divisor / 10;
        }
    }
    if (upper) {
        buffer[position] = 'E';
    } else {
        buffer[position] = 'e';
    }
    position = position + 1;
    if (exponent < 0) {
        buffer[position] = '-';
        position = position + 1;
        exponent = 0 - exponent;
    } else {
        buffer[position] = '+';
        position = position + 1;
    }
    char exp_digits[8];
    int exp_count = 0;
    if (exponent == 0) {
        exp_digits[exp_count] = '0';
        exp_count = exp_count + 1;
    }
    while (exponent > 0) {
        exp_digits[exp_count] = (char)('0' + (exponent % 10));
        exp_count = exp_count + 1;
        exponent = exponent / 10;
    }
    if (exp_count < 2) {
        buffer[position] = '0';
        position = position + 1;
    }
    while (exp_count > 0) {
        exp_count = exp_count - 1;
        buffer[position] = exp_digits[exp_count];
        position = position + 1;
    }
    return position;
}

static int decimal_exponent(double value) {
    int exponent = 0;
    if (value < 0.0) {
        value = 0.0 - value;
    }
    if (value == 0.0) {
        return 0;
    }
    while (value >= 10.0) {
        value = value / 10.0;
        exponent = exponent + 1;
    }
    while (value < 1.0) {
        value = value * 10.0;
        exponent = exponent - 1;
    }
    return exponent;
}

static int strip_float(char *buffer, int length, int alternate) {
    if (alternate) {
        return length;
    }
    int exponent_position = -1;
    int dot_position = -1;
    int index = 0;
    while (index < length) {
        if (buffer[index] == 'e' || buffer[index] == 'E') {
            exponent_position = index;
            break;
        }
        if (buffer[index] == '.') {
            dot_position = index;
        }
        index = index + 1;
    }
    if (dot_position < 0) {
        return length;
    }
    int end = length;
    if (exponent_position >= 0) {
        end = exponent_position;
    }
    while (end > dot_position + 1 && buffer[end - 1] == '0') {
        end = end - 1;
    }
    if (end == dot_position + 1) {
        end = dot_position;
    }
    if (exponent_position >= 0) {
        int tail = exponent_position;
        while (tail < length) {
            buffer[end] = buffer[tail];
            end = end + 1;
            tail = tail + 1;
        }
        length = end;
    } else {
        length = end;
    }
    return length;
}

static int format_double_general(char *buffer, double value, int precision, int flags) {
    int alternate = (flags >> 2) & 1;
    if (precision < 0) {
        precision = 6;
    }
    if (precision == 0) {
        precision = 1;
    }
    int exponent = decimal_exponent(value);
    int length = 0;
    if (exponent < -4 || exponent >= precision) {
        length = format_double_exp(buffer, value, precision - 1, flags);
    } else {
        int fraction = precision - exponent - 1;
        if (fraction < 0) {
            fraction = 0;
        }
        length = format_double_fixed(buffer, value, fraction, flags);
    }
    return strip_float(buffer, length, alternate);
}

double sqrt(double value) {
    return __builtin_sqrt(value);
}

float sqrtf(float value) {
    return __builtin_sqrt(value);
}

double fabs(double value) {
    return __builtin_fabs(value);
}

float fabsf(float value) {
    return __builtin_fabs(value);
}

double fmin(double left, double right) {
    return __builtin_fmin(left, right);
}

float fminf(float left, float right) {
    return __builtin_fmin(left, right);
}

double fmax(double left, double right) {
    return __builtin_fmax(left, right);
}

float fmaxf(float left, float right) {
    return __builtin_fmax(left, right);
}

char *strcpy(char *destination, const char *source) {
    char *result = destination;
    while (*source != '\0') {
        *destination = *source;
        destination = destination + 1;
        source = source + 1;
    }
    *destination = '\0';
    return result;
}

char *strncpy(char *destination, const char *source, int count) {
    char *result = destination;
    int index = 0;
    while (index < count && source[index] != '\0') {
        destination[index] = source[index];
        index = index + 1;
    }
    while (index < count) {
        destination[index] = '\0';
        index = index + 1;
    }
    return result;
}

char *strcat(char *destination, const char *source) {
    char *result = destination;
    while (*destination != '\0') {
        destination = destination + 1;
    }
    strcpy(destination, source);
    return result;
}

char *strncat(char *destination, const char *source, int count) {
    char *result = destination;
    while (*destination != '\0') {
        destination = destination + 1;
    }
    int index = 0;
    while (index < count && source[index] != '\0') {
        destination[index] = source[index];
        index = index + 1;
    }
    destination[index] = '\0';
    return result;
}

int strcmp(const char *left, const char *right) {
    int index = 0;
    while (left[index] != '\0' && left[index] == right[index]) {
        index = index + 1;
    }
    return (left[index] & 0xFF) - (right[index] & 0xFF);
}

int strncmp(const char *left, const char *right, int count) {
    int index = 0;
    while (index < count) {
        const int difference =
            (left[index] & 0xFF) - (right[index] & 0xFF);
        if (difference != 0 || left[index] == '\0') {
            return difference;
        }
        index = index + 1;
    }
    return 0;
}

char *strchr(const char *text, int character) {
    const char wanted = (char)character;
    while (*text != '\0') {
        if (*text == wanted) {
            return (char *)text;
        }
        text = text + 1;
    }
    if (wanted == '\0') {
        return (char *)text;
    }
    return 0;
}

char *strrchr(const char *text, int character) {
    const char wanted = (char)character;
    const char *last = 0;
    while (*text != '\0') {
        if (*text == wanted) {
            last = text;
        }
        text = text + 1;
    }
    if (wanted == '\0') {
        return (char *)text;
    }
    return (char *)last;
}

char *strstr(const char *text, const char *needle) {
    if (*needle == '\0') {
        return (char *)text;
    }
    while (*text != '\0') {
        const char *left = text;
        const char *right = needle;
        while (*right != '\0' && *left == *right) {
            left = left + 1;
            right = right + 1;
        }
        if (*right == '\0') {
            return (char *)text;
        }
        text = text + 1;
    }
    return 0;
}

static int is_delimiter(char character, const char *delimiters) {
    while (*delimiters != '\0') {
        if (*delimiters == character) {
            return 1;
        }
        delimiters = delimiters + 1;
    }
    return 0;
}

int strspn(const char *text, const char *accept) {
    int count = 0;
    while (text[count] != '\0' && is_delimiter(text[count], accept)) {
        count = count + 1;
    }
    return count;
}

int strcspn(const char *text, const char *reject) {
    int count = 0;
    while (text[count] != '\0' && !is_delimiter(text[count], reject)) {
        count = count + 1;
    }
    return count;
}

int strcoll(const char *left, const char *right) {
    return strcmp(left, right);
}

int strxfrm(char *destination, const char *source, int count) {
    int length = 0;
    while (source[length] != '\0') {
        length = length + 1;
    }
    if (destination != 0) {
        int index = 0;
        while (index < length && index < count - 1) {
            destination[index] = source[index];
            index = index + 1;
        }
        if (count > 0) {
            destination[index] = '\0';
        }
    }
    return length;
}

char *strpbrk(const char *text, const char *accept) {
    int index = 0;
    while (text[index] != '\0') {
        if (is_delimiter(text[index], accept)) {
            return (char *)text + index;
        }
        index = index + 1;
    }
    return 0;
}

static char *token_cursor = 0;

char *strtok(char *text, const char *delimiters) {
    if (text != 0) {
        token_cursor = text;
    }
    if (token_cursor == 0) {
        return 0;
    }
    while (*token_cursor != '\0' && is_delimiter(*token_cursor, delimiters)) {
        token_cursor = token_cursor + 1;
    }
    if (*token_cursor == '\0') {
        token_cursor = 0;
        return 0;
    }
    char *result = token_cursor;
    while (*token_cursor != '\0' && !is_delimiter(*token_cursor, delimiters)) {
        token_cursor = token_cursor + 1;
    }
    if (*token_cursor != '\0') {
        *token_cursor = '\0';
        token_cursor = token_cursor + 1;
    } else {
        token_cursor = 0;
    }
    return result;
}

char *strtok_r(char *text, const char *delimiters, char **save) {
    char *cursor = text;
    if (cursor == 0) {
        cursor = *save;
    }
    if (cursor == 0) {
        return 0;
    }
    while (*cursor != '\0' && is_delimiter(*cursor, delimiters)) {
        cursor = cursor + 1;
    }
    if (*cursor == '\0') {
        *save = 0;
        return 0;
    }
    char *result = cursor;
    while (*cursor != '\0' && !is_delimiter(*cursor, delimiters)) {
        cursor = cursor + 1;
    }
    if (*cursor != '\0') {
        *cursor = '\0';
        *save = cursor + 1;
    } else {
        *save = 0;
    }
    return result;
}

int strlen(const char *s);

char *strdup(const char *text) {
    const int length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (copy == 0) {
        return 0;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

int strnlen(const char *text, int count) {
    int length = 0;
    while (length < count && text[length] != '\0') {
        length = length + 1;
    }
    return length;
}

char *strndup(const char *text, int count) {
    const int length = strnlen(text, count);
    char *copy = (char *)malloc(length + 1);
    if (copy == 0) {
        return 0;
    }
    int index = 0;
    while (index < length) {
        copy[index] = text[index];
        index = index + 1;
    }
    copy[length] = '\0';
    return copy;
}

void *memmove(void *destination, const void *source, int count) {
    char *to = (char *)destination;
    const char *from = (const char *)source;
    if (count <= 0 || to == from) {
        return destination;
    }
    if ((uintptr_t)to < (uintptr_t)from) {
        int index = 0;
        while (index < count) {
            to[index] = from[index];
            index = index + 1;
        }
    } else {
        int index = count;
        while (index > 0) {
            index = index - 1;
            to[index] = from[index];
        }
    }
    return destination;
}

void *memccpy(void *destination, const void *source, int character, int count) {
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    int index = 0;
    while (index < count) {
        to[index] = from[index];
        if (from[index] == (unsigned char)character) {
            return to + index + 1;
        }
        index = index + 1;
    }
    return 0;
}

int memcmp(const void *left, const void *right, int count) {
    const char *a = (const char *)left;
    const char *b = (const char *)right;
    int index = 0;
    while (index < count) {
        const int difference = (a[index] & 0xFF) - (b[index] & 0xFF);
        if (difference != 0) {
            return difference;
        }
        index = index + 1;
    }
    return 0;
}

void *memchr(const void *memory, int character, int count) {
    const char *bytes = (const char *)memory;
    const char wanted = (char)character;
    int index = 0;
    while (index < count) {
        if (bytes[index] == wanted) {
            return (void *)(bytes + index);
        }
        index = index + 1;
    }
    return 0;
}

static int format_output(const char *format, va_list arguments) {
    int total = 0;
    const char *cursor = format;
    char buffer[160];
    while (*cursor != '\0') {
        if (*cursor != '%') {
            emit_character((int)*cursor);
            total = total + 1;
            cursor = cursor + 1;
            continue;
        }
        cursor = cursor + 1;
        if (*cursor == '%') {
            emit_character('%');
            total = total + 1;
            cursor = cursor + 1;
            continue;
        }
        int left = 0;
        int plus = 0;
        int space = 0;
        int alternate = 0;
        int zero = 0;
        while (*cursor == '-' || *cursor == '+' || *cursor == ' ' ||
               *cursor == '#' || *cursor == '0') {
            if (*cursor == '-') left = 1;
            if (*cursor == '+') plus = 1;
            if (*cursor == ' ') space = 1;
            if (*cursor == '#') alternate = 1;
            if (*cursor == '0') zero = 1;
            cursor = cursor + 1;
        }
        int width = 0;
        if (*cursor == '*') {
            width = va_arg(arguments, int);
            cursor = cursor + 1;
            if (width < 0) {
                left = 1;
                width = 0 - width;
            }
        } else {
            while (*cursor >= '0' && *cursor <= '9') {
                width = width * 10 + (*cursor - '0');
                cursor = cursor + 1;
            }
        }
        int precision = -1;
        if (*cursor == '.') {
            cursor = cursor + 1;
            precision = 0;
            if (*cursor == '*') {
                precision = va_arg(arguments, int);
                cursor = cursor + 1;
                if (precision < 0) {
                    precision = -1;
                }
            } else {
                while (*cursor >= '0' && *cursor <= '9') {
                    precision = precision * 10 + (*cursor - '0');
                    cursor = cursor + 1;
                }
            }
        }
        int long_count = 0;
        while (*cursor == 'h' || *cursor == 'l' || *cursor == 'j' ||
               *cursor == 'z' || *cursor == 't' || *cursor == 'L') {
            if (*cursor == 'l') {
                long_count = long_count + 1;
            }
            cursor = cursor + 1;
        }
        char spec = *cursor;
        if (spec != '\0') {
            cursor = cursor + 1;
        }
        int length = 0;
        int prefix = 0;
        if (spec == 'd' || spec == 'i') {
            int integer_flags = 0;
            if (long_count >= 2) {
                long long value = va_arg(arguments, long long);
                unsigned long long magnitude = 0;
                int negative = value < 0;
                if (negative) {
                    magnitude = (unsigned long long)0 - (unsigned long long)value;
                    integer_flags = integer_flags | 1;
                } else {
                    magnitude = (unsigned long long)value;
                }
                if (plus) integer_flags = integer_flags | 2;
                if (space) integer_flags = integer_flags | 4;
                length = format_integer64(buffer, magnitude,
                                          integer_flags | (10 << 8));
                if (negative || plus || space) {
                    prefix = 1;
                }
            } else {
                int value = va_arg(arguments, int);
                unsigned int magnitude = 0;
                int negative = value < 0;
                if (negative) {
                    magnitude = (uintptr_t)(0 - value);
                    integer_flags = integer_flags | 1;
                } else {
                    magnitude = (uintptr_t)value;
                }
                if (plus) integer_flags = integer_flags | 2;
                if (space) integer_flags = integer_flags | 4;
                length = format_integer(buffer, magnitude, 10, integer_flags);
                if (negative || plus || space) {
                    prefix = 1;
                }
            }
        } else if (spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o') {
            int base = 10;
            int integer_flags = 0;
            if (spec == 'x' || spec == 'X') {
                base = 16;
                if (alternate) integer_flags = integer_flags | 8;
                if (spec == 'X') integer_flags = integer_flags | 16;
            } else if (spec == 'o') {
                base = 8;
                if (alternate) integer_flags = integer_flags | 8;
            }
            if (long_count >= 2) {
                unsigned long long value =
                    va_arg(arguments, unsigned long long);
                length = format_integer64(buffer, value,
                                          integer_flags | (base << 8));
                if ((spec == 'x' || spec == 'X') && alternate && value != 0) {
                    prefix = 2;
                } else if (spec == 'o' && alternate && value != 0) {
                    prefix = 1;
                }
            } else {
                unsigned int value = va_arg(arguments, unsigned int);
                length = format_integer(buffer, value, base, integer_flags);
                if ((spec == 'x' || spec == 'X') && alternate && value != 0) {
                    prefix = 2;
                } else if (spec == 'o' && alternate && value != 0) {
                    prefix = 1;
                }
            }
        } else if (spec == 'p') {
            unsigned int value = (uintptr_t)va_arg(arguments, void *);
            length = format_integer(buffer, value, 16, 8);
            prefix = 2;
        } else if (spec == 'c') {
            buffer[0] = (char)va_arg(arguments, int);
            length = 1;
        } else if (spec == 's') {
            const char *text = va_arg(arguments, const char *);
            if (text == 0) {
                text = "(null)";
            }
            length = 0;
            while (text[length] != '\0' && (precision < 0 || length < precision)) {
                length = length + 1;
            }
            int field_flags = 0;
            if (left) {
                field_flags = 1;
            }
            total = total + out_field(text, length, width, field_flags);
            continue;
        } else if (spec == 'f' || spec == 'F') {
            double value = va_arg(arguments, double);
            int float_flags = 0;
            if (plus) float_flags = float_flags | 1;
            if (space) float_flags = float_flags | 2;
            if (alternate) float_flags = float_flags | 4;
            int fp_precision = precision;
            if (fp_precision < 0) {
                fp_precision = 6;
            }
            length = format_double_fixed(buffer, value, fp_precision, float_flags);
            if (value < 0.0 || plus || space) {
                prefix = 1;
            }
        } else if (spec == 'e' || spec == 'E') {
            double value = va_arg(arguments, double);
            int float_flags = 0;
            if (plus) float_flags = float_flags | 1;
            if (space) float_flags = float_flags | 2;
            if (alternate) float_flags = float_flags | 4;
            if (spec == 'E') float_flags = float_flags | 8;
            int fp_precision = precision;
            if (fp_precision < 0) {
                fp_precision = 6;
            }
            length = format_double_exp(buffer, value, fp_precision, float_flags);
            if (value < 0.0 || plus || space) {
                prefix = 1;
            }
        } else if (spec == 'g' || spec == 'G') {
            double value = va_arg(arguments, double);
            int float_flags = 0;
            if (plus) float_flags = float_flags | 1;
            if (space) float_flags = float_flags | 2;
            if (alternate) float_flags = float_flags | 4;
            if (spec == 'G') float_flags = float_flags | 8;
            int fp_precision = precision;
            if (fp_precision < 0) {
                fp_precision = 6;
            }
            length = format_double_general(buffer, value, fp_precision, float_flags);
            if (value < 0.0 || plus || space) {
                prefix = 1;
            }
        } else {
            buffer[0] = '%';
            buffer[1] = spec;
            length = 2;
        }
        int field_flags = 0;
        if (left) field_flags = field_flags | 1;
        if (zero) field_flags = field_flags | 2;
        field_flags = field_flags | (prefix << 8);
        total = total + out_field(buffer, length, width, field_flags);
    }
    return total;
}

int vfprintf(struct kr32_file *stream, const char *format, va_list arguments) {
    format_target = 1;
    format_stream = stream;
    format_sink = 0;
    return format_output(format, arguments);
}

int fprintf(struct kr32_file *stream, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    format_target = 1;
    format_stream = stream;
    format_sink = 0;
    const int result = format_output(format, arguments);
    va_end(arguments);
    return result;
}

int vprintf(const char *format, va_list arguments) {
    format_target = 0;
    format_stream = 0;
    format_sink = 0;
    return format_output(format, arguments);
}

int printf(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    format_target = 0;
    format_stream = 0;
    format_sink = 0;
    const int result = format_output(format, arguments);
    va_end(arguments);
    return result;
}

int vsnprintf(char *buffer, int size, const char *format, va_list arguments) {
    struct string_sink sink;
    sink.buffer = buffer;
    sink.capacity = size;
    sink.length = 0;
    format_target = 2;
    format_stream = 0;
    format_sink = &sink;
    const int result = format_output(format, arguments);
    if (size > 0 && buffer != 0) {
        int end = sink.length;
        if (end > size - 1) {
            end = size - 1;
        }
        buffer[end] = '\0';
    }
    return result;
}

int snprintf(char *buffer, int size, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return result;
}

int vsprintf(char *buffer, const char *format, va_list arguments) {
    struct string_sink sink;
    sink.buffer = buffer;
    sink.capacity = -1;
    sink.length = 0;
    format_target = 2;
    format_stream = 0;
    format_sink = &sink;
    const int result = format_output(format, arguments);
    if (buffer != 0) {
        buffer[sink.length] = '\0';
    }
    return result;
}

int sprintf(char *buffer, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vsprintf(buffer, format, arguments);
    va_end(arguments);
    return result;
}

static int tmpfile_counter = 0;

struct kr32_file *tmpfile(void) {
    char path[64];
    tmpfile_counter = tmpfile_counter + 1;
    snprintf(path, 64, "kr32-tmp-%d.tmp", tmpfile_counter);
    struct kr32_file *stream = fopen(path, "w+");
    if (stream == 0) {
        return 0;
    }
    (void)remove(path);
    return stream;
}

int isdigit(int c) { return c >= '0' && c <= '9'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int isalpha(int c) { return islower(c) || isupper(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int isprint(int c) { return c >= 32 && c <= 126; }
int isgraph(int c) { return c > 32 && c <= 126; }
int ispunct(int c) { return isprint(c) && c != ' ' && !isalnum(c); }
int tolower(int c) { return isupper(c) ? c + 32 : c; }
int toupper(int c) { return islower(c) ? c - 32 : c; }

int __kr32_assert_fail(void) {
    write(2, "assertion failed\n", 17);
    exit(1);
    return 1;
}

int atoi(const char *text) { return (int)atol(text); }

long atol(const char *text) { return strtol(text, 0, 10); }

long strtol(const char *text, char **end, int base) {
    const char *cursor = text;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' ||
           *cursor == '\r' || *cursor == '\v' || *cursor == '\f') {
        cursor = cursor + 1;
    }
    int negative = 0;
    if (*cursor == '-' || *cursor == '+') {
        negative = *cursor == '-';
        cursor = cursor + 1;
    }
    if (base == 0) {
        base = 10;
        if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
            base = 16;
            cursor = cursor + 2;
        } else if (cursor[0] == '0') {
            base = 8;
        }
    } else if (base == 16 && cursor[0] == '0' &&
               (cursor[1] == 'x' || cursor[1] == 'X')) {
        cursor = cursor + 2;
    }
    long value = 0;
    int digits = 0;
    while (*cursor != 0) {
        int digit = -1;
        if (*cursor >= '0' && *cursor <= '9') {
            digit = *cursor - '0';
        } else if (*cursor >= 'a' && *cursor <= 'z') {
            digit = *cursor - 'a' + 10;
        } else if (*cursor >= 'A' && *cursor <= 'Z') {
            digit = *cursor - 'A' + 10;
        }
        if (digit < 0 || digit >= base) {
            break;
        }
        value = value * base + digit;
        digits = digits + 1;
        cursor = cursor + 1;
    }
    if (end != 0) {
        *end = (char *)(digits > 0 ? cursor : text);
    }
    return negative ? -value : value;
}

unsigned long strtoul(const char *text, char **end, int base) {
    return (unsigned long)strtol(text, end, base);
}

long long strtoll(const char *text, char **end, int base) {
    const char *cursor = text;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') {
        cursor = cursor + 1;
    }
    int negative = 0;
    if (*cursor == '-' || *cursor == '+') {
        negative = *cursor == '-';
        cursor = cursor + 1;
    }
    if (base == 0) {
        base = 10;
        if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
            base = 16;
            cursor = cursor + 2;
        } else if (cursor[0] == '0') {
            base = 8;
        }
    } else if (base == 16 && cursor[0] == '0' &&
               (cursor[1] == 'x' || cursor[1] == 'X')) {
        cursor = cursor + 2;
    }
    unsigned long long value = 0;
    int digits = 0;
    while (*cursor != 0) {
        int digit = -1;
        if (*cursor >= '0' && *cursor <= '9') {
            digit = *cursor - '0';
        } else if (*cursor >= 'a' && *cursor <= 'z') {
            digit = *cursor - 'a' + 10;
        } else if (*cursor >= 'A' && *cursor <= 'Z') {
            digit = *cursor - 'A' + 10;
        }
        if (digit < 0 || digit >= base) {
            break;
        }
        value = value * (unsigned long long)base + (unsigned long long)digit;
        digits = digits + 1;
        cursor = cursor + 1;
    }
    if (end != 0) {
        *end = (char *)(digits > 0 ? cursor : text);
    }
    if (negative) {
        return -(long long)value;
    }
    return (long long)value;
}

unsigned long long strtoull(const char *text, char **end, int base) {
    return (unsigned long long)strtoll(text, end, base);
}

double strtod(const char *text, char **end) {
    const char *cursor = text;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') {
        cursor = cursor + 1;
    }
    int negative = 0;
    if (*cursor == '-' || *cursor == '+') {
        negative = *cursor == '-';
        cursor = cursor + 1;
    }
    double value = 0.0;
    while (*cursor >= '0' && *cursor <= '9') {
        value = value * 10.0 + (*cursor - '0');
        cursor = cursor + 1;
    }
    if (*cursor == '.') {
        cursor = cursor + 1;
        double scale = 0.1;
        while (*cursor >= '0' && *cursor <= '9') {
            value = value + (*cursor - '0') * scale;
            scale = scale * 0.1;
            cursor = cursor + 1;
        }
    }
    if (*cursor == 'e' || *cursor == 'E') {
        const char *exponent_cursor = cursor + 1;
        int exponent_negative = 0;
        if (*exponent_cursor == '-' || *exponent_cursor == '+') {
            exponent_negative = *exponent_cursor == '-';
            exponent_cursor = exponent_cursor + 1;
        }
        if (*exponent_cursor >= '0' && *exponent_cursor <= '9') {
            int exponent = 0;
            while (*exponent_cursor >= '0' && *exponent_cursor <= '9') {
                exponent = exponent * 10 + (*exponent_cursor - '0');
                exponent_cursor = exponent_cursor + 1;
            }
            double scale = 1.0;
            int index = 0;
            while (index < exponent) {
                scale = scale * 10.0;
                index = index + 1;
            }
            if (exponent_negative) {
                value = value / scale;
            } else {
                value = value * scale;
            }
            cursor = exponent_cursor;
        }
    }
    if (end != 0) {
        *end = (char *)cursor;
    }
    return negative ? -value : value;
}

float strtof(const char *text, char **end) {
    return (float)strtod(text, end);
}

double strtold(const char *text, char **end) {
    return strtod(text, end);
}

double atof(const char *text) {
    return strtod(text, (char **)0);
}

int time(int *result) {
    int value = sys_time();
    if (result != 0) {
        *result = value;
    }
    return value;
}

double difftime(double later, double earlier) {
    return later - earlier;
}

int abs(int value) { return value < 0 ? -value : value; }

long labs(long value) { return value < 0 ? -value : value; }

long long llabs(long long value) { return value < 0 ? -value : value; }

static unsigned int rand_state = 1;

void srand(unsigned int seed) { rand_state = seed; }

int rand(void) {
    rand_state = rand_state * 1103515245U + 12345U;
    return (int)((rand_state >> 16) & 0x7FFFU);
}

void qsort(void *base, int count, int size, kr32_compare_fn compare) {
    char *bytes = (char *)base;
    int index = 1;
    while (index < count) {
        int position = index;
        while (position > 0 &&
               compare(bytes + (position - 1) * size, bytes + position * size) > 0) {
            int offset = 0;
            while (offset < size) {
                char temporary = bytes[(position - 1) * size + offset];
                bytes[(position - 1) * size + offset] = bytes[position * size + offset];
                bytes[position * size + offset] = temporary;
                offset = offset + 1;
            }
            position = position - 1;
        }
        index = index + 1;
    }
}

void *bsearch(const void *key, const void *base, int count, int size,
              kr32_compare_fn compare) {
    const char *bytes = (const char *)base;
    int low = 0;
    int high = count - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        int result = compare(key, bytes + middle * size);
        if (result == 0) {
            return (void *)(bytes + middle * size);
        }
        if (result < 0) {
            high = middle - 1;
        } else {
            low = middle + 1;
        }
    }
    return 0;
}
