#include "console.h"
#include "initrd.h"
#include "memory.h"
#include "page.h"
#include "process.h"
#include "syscall.h"
#include "user_memory.h"

const unsigned char *initrd_address(void);

void kernel_enter_user(unsigned int entry, unsigned int stack);

typedef mem_phys_t user_frame_list_t[16]; /* USER_MAX_PAGES */

mem_phys_t user_virt_to_phys(mem_virt_t address) {
    unsigned int *directory =
        (unsigned int *)mem_phys_to_virt(USER_PAGE_DIRECTORY_PHYS);
    const unsigned int directory_index = address >> PAGE_DIRECTORY_SHIFT;
    const unsigned int directory_entry = directory[directory_index];
    if ((directory_entry & (PAGE_PRESENT | PAGE_USER)) !=
        (PAGE_PRESENT | PAGE_USER)) {
        return MEM_PHYS_NONE;
    }
    unsigned int *table =
        (unsigned int *)mem_phys_to_virt(directory_entry & PAGE_FRAME_MASK);
    const unsigned int table_index =
        (address >> PAGE_TABLE_SHIFT) & PAGE_TABLE_INDEX_MASK;
    const unsigned int table_entry = table[table_index];
    if ((table_entry & (PAGE_PRESENT | PAGE_USER)) !=
        (PAGE_PRESENT | PAGE_USER)) {
        return MEM_PHYS_NONE;
    }
    return (table_entry & PAGE_FRAME_MASK) | (address & PAGE_OFFSET_MASK);
}

static void setup_user_pages(void) {
    unsigned int *directory =
        (unsigned int *)mem_phys_to_virt(USER_PAGE_DIRECTORY_PHYS);
    unsigned int *code =
        (unsigned int *)mem_phys_to_virt(USER_CODE_PAGE_TABLE_PHYS);
    unsigned int *stack =
        (unsigned int *)mem_phys_to_virt(USER_STACK_PAGE_TABLE_PHYS);

    unsigned int index = 0;
    while (index < PAGE_TABLE_ENTRIES) {
        code[index] = 0;
        ++index;
    }
    directory[USER_CODE_PDE_INDEX] =
        USER_CODE_PAGE_TABLE_PHYS | PAGE_PRESENT | PAGE_USER;

    // Stack virtual pages 0x07E01000..0x07EFFFFF map the same physical
    // window; 0x07E00000 remains the guard page.
    for (index = USER_STACK_FIRST_PTE_INDEX;
         index < USER_STACK_LAST_PTE_INDEX; ++index) {
        stack[index] =
            (USER_STACK_PHYS_BASE + index * PAGE_SIZE) | PAGE_USER_RW;
    }
    stack[USER_STACK_GUARD_PTE_INDEX] = 0;
    directory[USER_STACK_PDE_INDEX] =
        USER_STACK_PAGE_TABLE_PHYS | PAGE_PRESENT | PAGE_USER;
}

int run_named(const char *name);

int run_init(void) {
    return run_named("init");
}

static int name_equal(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) return 0;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static const struct initrd_header *initrd_header(void) {
    const struct initrd_header *header =
        (const struct initrd_header *)initrd_address();
    if (header->magic != INITRD_MAGIC || header->version != INITRD_VERSION) {
        return 0;
    }
    return header;
}

static const struct initrd_entry *initrd_entries(void) {
    const struct initrd_header *header = initrd_header();
    if (header == 0) {
        return 0;
    }
    return (const struct initrd_entry *)((const unsigned char *)header +
                                         sizeof(struct initrd_header));
}

static const struct initrd_entry *find_entry(const char *name) {
    const struct initrd_header *header = initrd_header();
    const struct initrd_entry *entries = initrd_entries();
    if (header == 0 || entries == 0) {
        return 0;
    }
    unsigned int index = 0;
    while (index < header->count) {
        if (name_equal(entries[index].name, name)) {
            return entries + index;
        }
        ++index;
    }
    return 0;
}

void initrd_list(void) {
    const struct initrd_header *header = initrd_header();
    const struct initrd_entry *entries = initrd_entries();
    if (header == 0 || entries == 0) {
        return;
    }
    unsigned int index = 0;
    while (index < header->count) {
        console_write(entries[index].name);
        console_putc('\n');
        ++index;
    }
}

int initrd_cat(const char *name) {
    const struct initrd_entry *entry = find_entry(name);
    if (entry == 0) {
        return -1;
    }
    const unsigned char *data = initrd_address() + entry->offset;
    unsigned int index = 0;
    while (index < entry->size) {
        console_putc((char)data[index]);
        ++index;
    }
    if (entry->size == 0 || data[entry->size - 1] != '\n') {
        console_putc('\n');
    }
    return 0;
}

int run_named(const char *name) {
    const struct initrd_entry *entry = find_entry(name);
    if (entry == 0) {
        return -1;
    }
    const unsigned int code_limit = USER_MAX_PAGES * PAGE_SIZE;
    if (entry->size == 0 || entry->size > code_limit ||
        entry->entry < USER_CODE_VA ||
        entry->entry >= USER_CODE_VA + code_limit) {
        return -1;
    }

    setup_user_pages();
    unsigned int *code =
        (unsigned int *)mem_phys_to_virt(USER_CODE_PAGE_TABLE_PHYS);
    const unsigned int pages =
        (entry->size + PAGE_OFFSET_MASK) / PAGE_SIZE;
    user_frame_list_t frames;
    const unsigned char *source = initrd_address() + entry->offset;

    unsigned int index = 0;
    while (index < pages) {
        const mem_phys_t frame = mem_page_alloc_phys();
        if (frame == MEM_PHYS_NONE) {
            while (index > 0) {
                --index;
                mem_page_free_phys(frames[index]);
            }
            return -1;
        }
        frames[index] = frame;
        // Map the frame writable just long enough for the kernel to copy the
        // image through the user virtual address; then drop W before entry.
        code[index] = frame | PAGE_USER_RWX;

        unsigned char *destination =
            (unsigned char *)(USER_CODE_VA + (index << PAGE_TABLE_SHIFT));
        const unsigned int start = index << PAGE_TABLE_SHIFT;
        unsigned int offset = 0;
        while (offset < PAGE_SIZE && start + offset < entry->size) {
            destination[offset] = source[start + offset];
            ++offset;
        }
        code[index] = frame | PAGE_USER_RX;
        ++index;
    }

    volatile struct syscall_scratch *scratch =
        (volatile struct syscall_scratch *)SYSCALL_SCRATCH_PHYS;
    scratch->result = 0;
    int pid = process_spawn(name);
    if (pid < 0) {
        index = 0;
        while (index < pages) {
            mem_page_free_phys(frames[index]);
            ++index;
        }
        return pid;
    }
    kernel_enter_user(entry->entry, USER_STACK_TOP_VA);
    int status = (int)scratch->result;
    (void)process_exit(pid, status);
    (void)process_reap(pid);

    index = 0;
    while (index < pages) {
        mem_page_free_phys(frames[index]);
        ++index;
    }
    return status;
}
