/* allocator.c — Simple coalescing free-list allocator (plan.md §14.3).
 *
 * Uses HAL-provided heap bounds (krc_hal_alloc/krc_hal_free) to manage a
 * simple bump-pointer allocator with coalescing free list. Designed to be
 * compilable by both the host compiler and krc once the front end supports
 * the needed constructs.
 */

#include "hal.h"

#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
#  ifndef bool
#    define bool _Bool
#    define true 1
#    define false 0
#  endif
#endif

/* Heap management: the linker provides __heap_start and __heap_end symbols.
 * These are declared extern and resolved at link time. */
extern char __heap_start[];
extern char __heap_end[];

typedef struct Block {
    struct Block* next;
    size_t size;
} Block;

static Block* free_list = NULL;
static char* heap_ptr = NULL;
static bool heap_initialized = false;

static void init_heap(void) {
    heap_ptr = __heap_start;
    free_list = NULL;
    heap_initialized = true;
}

/* Find a free block large enough for size bytes. */
static Block* find_free_block(size_t size) {
    Block* prev = NULL;
    Block* curr = free_list;
    while (curr != NULL) {
        if (curr->size >= size) {
            if (prev != NULL) {
                prev->next = curr->next;
            } else {
                free_list = curr->next;
            }
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

void* krc_hal_alloc(size_t n) {
    if (n == 0) return NULL;
    if (!heap_initialized) init_heap();

    /* Align to 8 bytes on KR32, 2 bytes on KR16. */
    size_t align = 8;
    n = (n + align - 1) & ~(align - 1);

    Block* block = find_free_block(n);
    if (block != NULL) {
        return (void*)(block + 1);
    }

    /* Bump-pointer allocation from the heap. */
    size_t total = sizeof(Block) + n;
    if (heap_ptr + total > __heap_end) return NULL;

    block = (Block*)heap_ptr;
    block->size = n;
    block->next = NULL;
    heap_ptr += total;
    return (void*)(block + 1);
}

void krc_hal_free(void* ptr) {
    if (ptr == NULL) return;
    if (!heap_initialized) return;

    Block* block = ((Block*)ptr) - 1;
    block->next = free_list;
    free_list = block;
}
