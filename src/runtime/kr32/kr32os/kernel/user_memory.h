#pragma once

#include "memory.h"
#include "page.h"

/* User-visible virtual addresses. */
#define USER_CODE_VA 0x00400000U
#define USER_STACK_GUARD_VA 0x07E00000U
#define USER_STACK_TOP_VA 0x07F00000U
#define USER_STACK_FIRST_VA 0x07E01000U

/* Physical frames holding the user page tables. */
#define USER_PAGE_DIRECTORY_PHYS 0x00040000U
#define USER_CODE_PAGE_TABLE_PHYS 0x00070000U
#define USER_STACK_PAGE_TABLE_PHYS 0x00071000U

/* Physical window the identity-mapped user stack comes from. */
#define USER_STACK_PHYS_BASE 0x07C00000U

/* PDE/PTE indices for the two user mappings. */
#define USER_CODE_PDE_INDEX 1U
#define USER_STACK_PDE_INDEX 31U
#define USER_STACK_GUARD_PTE_INDEX 0x200U /* guard VA >> 12 */
#define USER_STACK_FIRST_PTE_INDEX 0x201U
#define USER_STACK_LAST_PTE_INDEX 0x300U  /* exclusive */

#define USER_MAX_PAGES 16U

// Translate a user virtual address through the active user page directory.
// Returns MEM_PHYS_NONE when the page is not present or not user-accessible.
mem_phys_t user_virt_to_phys(mem_virt_t address);
