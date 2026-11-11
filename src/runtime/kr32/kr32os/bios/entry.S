.file "kr32os/bios/entry.S"

; BootInfo contract.
.equ BOOTINFO_OFFSET_MAGIC, 0
.equ BOOTINFO_OFFSET_VERSION, 4
.equ BOOTINFO_OFFSET_SIZE, 8
.equ BOOTINFO_MAGIC, 0x4232334B
.equ BOOTINFO_VERSION, 1
.equ BOOTINFO_SIZE, 56

; Low-memory boot layout.
.equ BIOS_STACK_TOP, 0x00100000
.equ PAGE_SIZE, 0x1000
.equ PAGE_TABLE_ENTRIES, 1024
.equ PAGE_DIRECTORY_PHYS, 0x00040000
.equ LOW_PAGE_TABLE_PHYS, 0x00041000
.equ HIGH_PAGE_TABLE_PHYS, 0x00042000
.equ VECTOR_TABLE_PHYS, 0x00050000
.equ KERNEL_ENTRY, 0x00020000

; Page permission bits.
.equ PAGE_KERNEL_RWX, 11
.equ PAGE_KERNEL_RW, 3
.equ PAGE_KERNEL_RX, 9

; Low identity map guards and special leaves.
.equ LOW_GUARD_PAGES, 16
.equ LOW_GUARD_PTE, 0x00041180
.equ HIGH_MMIO_LEAF, 0x00042C00
.equ MMIO_PAGE_COUNT, 6
.equ HIGH_ROM_LEAF, 0x00042FC0
.equ ROM_PAGE_COUNT, 16
.equ HIGH_PAGE_DIRECTORY_ENTRY, 0x00040FFC

; Framebuffer aperture tables.
.equ FRAMEBUFFER_TABLE_START, 0x00043000
.equ FRAMEBUFFER_PDE_START, 0x00040FC0
.equ FRAMEBUFFER_TABLE_COUNT, 8
.equ FRAMEBUFFER_PAGE_COUNT, 1024
.equ FRAMEBUFFER_BASE, 0xFC000000
.equ MMIO_BASE, 0xFFF00000
.equ ROM_BASE, 0xFFFF0000

; Vector table entries.
.equ VECTOR_TABLE_ENTRIES, 256

; KR32OS M1 BIOS: validate BootInfo, identity-map the low kernel window,
; install a default vector table, enable paging, and enter the kernel.
.word bios_entry

bios_entry:
    ; R2 is the loader-provided BootInfo pointer.
    LOAD R3, [R2 + BOOTINFO_OFFSET_MAGIC]
    MOV R4, #BOOTINFO_MAGIC          ; "K32B" little-endian
    CMP R3, R4
    JNZ .Lbad_boot
    LOAD R3, [R2 + BOOTINFO_OFFSET_VERSION]
    MOV R4, #BOOTINFO_VERSION
    CMP R3, R4
    JNZ .Lbad_boot
    LOAD R3, [R2 + BOOTINFO_OFFSET_SIZE] ; struct size
    MOV R4, #BOOTINFO_SIZE
    CMP R3, R4
    JNZ .Lbad_boot

    ; Move the BIOS stack into the low identity window before paging.
    MOV R1, #BIOS_STACK_TOP
    SETSP R1
    WRCR CR6, R1

    ; Low 4 MiB page table at 0x41000. Identity-map RAM.
    MOV R1, #LOW_PAGE_TABLE_PHYS
    MOV R2, #0
    MOV R3, #0
.Llow_pages:
    ADD R4, R3, #PAGE_KERNEL_RWX
    STORE R4, [R1]
    ADD R1, R1, #4
    ADD R3, R3, #PAGE_SIZE
    ADD R2, R2, #1
    CMP R2, #PAGE_TABLE_ENTRIES
    JLT .Llow_pages

    ; Leave the guard region and the stack guard page unmapped.
    MOV R1, #LOW_PAGE_TABLE_PHYS
    MOV R2, #0
.Lclear_guard:
    STORE R2, [R1]
    ADD R1, R1, #4
    ADD R2, R2, #1
    CMP R2, #LOW_GUARD_PAGES
    JLT .Lclear_guard
    MOV R1, #LOW_GUARD_PTE          ; PTE for 0x00060000
    MOV R2, #0
    STORE R2, [R1]

    ; Page directory entry 0 -> low page table.
    MOV R1, #LOW_PAGE_TABLE_PHYS
    ADD R1, R1, #1
    STORE R1, [PAGE_DIRECTORY_PHYS]

    ; High page table at 0x42000 for PDE 0x3FF
    ; (0xFFC0_0000..0xFFFF_FFFF): MMIO and ROM.
    MOV R1, #HIGH_MMIO_LEAF         ; leaf 0x300: 0xFFF0_0000
    MOV R2, #0
    MOV R3, #MMIO_BASE
.Lmmio_pages:
    ADD R4, R3, #PAGE_KERNEL_RW
    STORE R4, [R1]
    ADD R1, R1, #4
    ADD R3, R3, #PAGE_SIZE
    ADD R2, R2, #1
    CMP R2, #MMIO_PAGE_COUNT        ; timer, block, console, IRQ, fb control, serial
    JLT .Lmmio_pages

    ; ROM pages at leaf index 0x3F0.
    MOV R1, #HIGH_ROM_LEAF
    MOV R2, #0
    MOV R3, #ROM_BASE
.Lrom_pages:
    ADD R4, R3, #PAGE_KERNEL_RX
    STORE R4, [R1]
    ADD R1, R1, #4
    ADD R3, R3, #PAGE_SIZE
    ADD R2, R2, #1
    CMP R2, #ROM_PAGE_COUNT
    JLT .Lrom_pages
    MOV R1, #HIGH_PAGE_TABLE_PHYS
    ADD R1, R1, #1
    STORE R1, [HIGH_PAGE_DIRECTORY_ENTRY] ; PDE 0x3FF

    ; Framebuffer page tables at 0x43000..0x4A000 for 0xFC00_0000..0xFDFF_FFFF.
    MOV R8, #FRAMEBUFFER_TABLE_START
    MOV R9, #FRAMEBUFFER_PDE_START
    MOV R6, #FRAMEBUFFER_TABLE_COUNT
    MOV R3, #FRAMEBUFFER_BASE
.Lfb_tables:
    MOV R1, R8
    MOV R2, #0
.Lfb_pages:
    ADD R5, R3, #PAGE_KERNEL_RW
    STORE R5, [R1]
    ADD R1, R1, #4
    ADD R3, R3, #PAGE_SIZE
    ADD R2, R2, #1
    CMP R2, #FRAMEBUFFER_PAGE_COUNT
    JLT .Lfb_pages
    ADD R5, R8, #1
    STORE R5, [R9]
    ADD R8, R8, #PAGE_SIZE
    ADD R9, R9, #4
    SUB R6, R6, #1
    CMP R6, #0
    JNZ .Lfb_tables

    ; Default vector table at 0x50000 points at a halt stub.
    MOV R8, #VECTOR_TABLE_PHYS
    MOV R9, #0
    LEA R10, [.Ldefault_handler]
.Lvector_loop:
    STORE R10, [R8]
    ADD R8, R8, #4
    ADD R9, R9, #1
    CMP R9, #VECTOR_TABLE_ENTRIES
    JLT .Lvector_loop

    ; Control registers.
    MOV R1, #PAGE_DIRECTORY_PHYS
    WRCR CR8, R1                 ; PTBR
    MOV R1, #VECTOR_TABLE_PHYS
    WRCR CR3, R1                 ; VTBR
    MOV R1, #BIOS_STACK_TOP
    WRCR CR6, R1
    SETSP R1
    MOV R1, #1
    WRCR CR10, R1                ; enable paging

    ; The BIOS stays in kernel mode and jumps to the kernel entry.
    JMP KERNEL_ENTRY

.Ldefault_handler:
    MOV R0, #1
    HALT

.Lbad_boot:
    MOV R0, #2
    HALT
