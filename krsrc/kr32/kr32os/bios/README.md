# KR32OS BIOS

The BIOS is the ROM-resident boot firmware for KR32OS. It is versioned with the
OS because it defines the kernel handoff ABI.

## Planned files

```text
entry.S    reset vector, stack setup, privileged stubs, final IRET
bios.c     optional freestanding helpers for BootInfo and page tables
```

Assembly owns the reset path and anything that executes privileged instructions.
C helpers are allowed only where they stay freestanding and do not introduce
writable ROM data.

## Responsibilities

- Start from the ROM reset vector at `0xFFFF_0000`.
- Establish a low temporary stack.
- Validate the loader-provided `BootInfo`.
- Build the initial identity map and page tables.
- Install the vector table and set `VTBR`.
- Set `PTBR`, `KSP`, `SP`, and enable paging.
- Enter the kernel at `0x0002_0000` with an `IRET` frame.

## Constraints

- The complete BIOS must fit in the 64 KiB ROM.
- No host libc, no standard `_start`, and no host runtime.
- Writable `.data` and `.bss` cannot live in ROM. Keep state on the stack or in
  an explicitly reserved RAM block.
- `WRCR` and `IRET` remain assembly stubs even when surrounding logic is C.
- Move to a low mapped stack before enabling paging.
- The ROM mapping and vector table must be active before paging is enabled.

## Build

The planned build is:

```text
kr32 cc bios/bios.c -S -o build/bios.s
kr32 as bios/entry.S --object -o build/bios-entry.o
kr32 as build/bios.s --object -o build/bios.o
kr32 ld build/bios-entry.o build/bios.o --flat --base 0xFFFF0000 -o build/bios.bin
```

The flat BIOS is loaded by `kr32 os` through `--bios` or the sibling `bios.bin`
resolution rule.
