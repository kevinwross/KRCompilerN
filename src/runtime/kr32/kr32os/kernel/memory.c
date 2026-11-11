#include "memory.h"
#include "memory_map.h"

typedef unsigned int memory_bitmap_t[1024]; /* MEM_BITMAP_WORDS */
static memory_bitmap_t mem_physical_bitmap;
static unsigned int mem_used_count = 0;
#define KMALLOC_ALIGNMENT 8U
#define KMALLOC_ALIGN_MASK 7U

static mem_phys_t mem_heap_phys_next;
static mem_phys_t mem_heap_phys_start;
static mem_phys_t mem_heap_phys_end;

static int page_is_used(unsigned int page);

static void mark_used(unsigned int page) {
    if (page < MEM_TOTAL_PAGES) {
        if (page_is_used(page) == 0) {
            ++mem_used_count;
        }
        mem_physical_bitmap[page >> MEM_BITMAP_WORD_SHIFT] |=
            1U << (page & MEM_BITMAP_WORD_MASK);
    }
}

static void mark_free(unsigned int page) {
    if (page < MEM_TOTAL_PAGES) {
        if (page_is_used(page) != 0) {
            --mem_used_count;
        }
        mem_physical_bitmap[page >> MEM_BITMAP_WORD_SHIFT] &=
            ~(1U << (page & MEM_BITMAP_WORD_MASK));
    }
}

static int page_is_used(unsigned int page) {
    return page < MEM_TOTAL_PAGES &&
           (mem_physical_bitmap[page >> MEM_BITMAP_WORD_SHIFT] &
            (1U << (page & MEM_BITMAP_WORD_MASK))) != 0;
}

void memory_init(void) {
    unsigned int index = 0;
    unsigned int stack_first = 0;
    while (index < MEM_BITMAP_WORDS) {
        mem_physical_bitmap[index] = 0;
        ++index;
    }
    mem_used_count = 0;
    mem_heap_phys_start = MEM_HEAP_START;
    mem_heap_phys_end = MEM_HEAP_END;
    mem_heap_phys_next = mem_heap_phys_start;
    index = 0;
    while (index < MEM_LOW_RESERVED_PAGES) {
        mark_used(index);
        ++index;
    }
    stack_first = MEM_USER_STACK_WINDOW_START >> PAGE_SHIFT;
    index = stack_first;
    while (index < stack_first + MEM_USER_STACK_WINDOW_PAGES) {
        mark_used(index);
        ++index;
    }
}

mem_phys_t mem_page_alloc_phys(void) {
    unsigned int page = MEM_LOW_RESERVED_PAGES;
    while (page < MEM_TOTAL_PAGES) {
        if (!page_is_used(page)) {
            mark_used(page);
            return page << PAGE_SHIFT;
        }
        ++page;
    }
    return MEM_PHYS_NONE;
}

void mem_page_free_phys(mem_phys_t address) {
    if ((address & PAGE_OFFSET_MASK) == 0) {
        mark_free(address >> PAGE_SHIFT);
    }
}

void *mem_phys_to_virt(mem_phys_t physical) {
    return (void *)physical;
}

mem_phys_t mem_virt_to_phys(void *virtual_address) {
    return (mem_phys_t)virtual_address;
}

void *kmalloc(unsigned int size) {
    unsigned int aligned =
        (size + KMALLOC_ALIGN_MASK) & ~KMALLOC_ALIGN_MASK;
    if (aligned == 0 || mem_heap_phys_next + aligned > mem_heap_phys_end) {
        return 0;
    }
    void *result = mem_phys_to_virt(mem_heap_phys_next);
    mem_heap_phys_next += aligned;
    return result;
}

void kfree(void *pointer) {
    (void)pointer;
}

unsigned int memory_used_pages(void) {
    return mem_used_count;
}

unsigned int heap_used(void) {
    return mem_heap_phys_next - mem_heap_phys_start;
}
