#pragma once

typedef unsigned int mem_phys_t;
typedef unsigned int mem_virt_t;

#define MEM_PHYS_NONE 0xFFFFFFFFU

void memory_init(void);
mem_phys_t mem_page_alloc_phys(void);
void mem_page_free_phys(mem_phys_t page);
void *kmalloc(unsigned int size);
void kfree(void *pointer);
unsigned int memory_used_pages(void);
unsigned int heap_used(void);

// The kernel's low memory window is identity-mapped in M4. These helpers keep
// that assumption in one place so callers do not silently mix address spaces.
void *mem_phys_to_virt(mem_phys_t physical);
mem_phys_t mem_virt_to_phys(void *virtual_address);
