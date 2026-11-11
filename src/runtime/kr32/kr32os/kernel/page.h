#pragma once

/*
 * KR32 paging constants shared by the kernel and the BIOS-facing code.
 * Every page is 4 KiB and every page table has 1024 32-bit entries.
 */
#define PAGE_SIZE 4096U
#define PAGE_SHIFT 12U
#define PAGE_TABLE_ENTRIES 1024U
#define PAGE_TABLE_INDEX_MASK 0x3FFU
#define PAGE_OFFSET_MASK 0xFFFU
#define PAGE_FRAME_MASK 0xFFFFF000U
#define PAGE_DIRECTORY_SHIFT 22U
#define PAGE_TABLE_SHIFT 12U

/* PTE/PDE permission bits. */
#define PAGE_PRESENT 1U
#define PAGE_WRITABLE 2U
#define PAGE_USER 4U
#define PAGE_EXECUTABLE 8U

#define PAGE_KERNEL_RW 3U
#define PAGE_KERNEL_RX 9U
#define PAGE_KERNEL_RWX 11U
#define PAGE_USER_RW 7U
#define PAGE_USER_RX 13U
#define PAGE_USER_RWX 15U
