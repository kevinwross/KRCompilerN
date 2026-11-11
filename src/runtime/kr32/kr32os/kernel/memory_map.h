#pragma once

#include "page.h"

/* The bitmap covers the 128 MiB physical address space. */
#define MEM_TOTAL_BYTES (128U * 1024U * 1024U)
#define MEM_TOTAL_PAGES 32768U
#define MEM_BITMAP_BITS_PER_WORD 32U
#define MEM_BITMAP_WORDS 1024U
#define MEM_BITMAP_WORD_SHIFT 5U
#define MEM_BITMAP_WORD_MASK 31U

/* Low 4 MiB: BIOS, page tables, vectors, kernel image, initrd, and heap. */
#define MEM_LOW_RESERVED_PAGES 1024U

/* Bump heap window. */
#define MEM_HEAP_START 0x00300000U
#define MEM_HEAP_END 0x003F0000U

/* Identity-mapped physical window backing the user stack. */
#define MEM_USER_STACK_WINDOW_START 0x07E00000U
#define MEM_USER_STACK_WINDOW_PAGES 256U
