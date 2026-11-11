# KR32OS Design Notes

Status: draft for the interactive design session.

## Implementation status

M0-M4 are implemented through the first user process. Current implementation
notes:

- BootInfo is written by the host loader and carries the initrd range read from
  `kernel.meta`.
- User page tables use static M4 windows at `0x70000`/`0x71000`; user code
  frames come from the bitmap allocator, while the initial user stack remains
  an identity-mapped physical window. The bump heap remains in low memory.
- `memory.h` distinguishes `mem_phys_t` from `mem_virt_t`; the low-memory
  identity mapping is centralized in `mem_phys_to_virt`/`mem_virt_to_phys`.
  Page tables require physical frames (`PTBR`, PDEs, PTEs), while user pointers
  are translated through the current page directory by `user_virt_to_phys`.
- The process table is a fixed 256-entry array.
- Syscall vectors 64-69 are installed (exit, write, read, yield, getpid, brk).
- User faults terminate the process with status 139 instead of panicking the
  kernel.
- The OS build emits `.sym` sidecars for the raw `.bin` images and optional
  `.k32` containers. `gui --os` loads the sidecar for source-line debugging;
  assembly line records are emitted by `.file` directives in the assembly
  sources.


### Layout and MMIO constants

The C sources keep hardware and memory-layout knowledge out of call sites:

- `kernel/page.h` defines page size, table geometry, frame masks, and PTE/PDE
  permission bits.
- `kernel/memory_map.h` defines the bitmap size, low-memory reservation, heap
  window, and identity-mapped user-stack window.
- `kernel/user_memory.h` defines user virtual addresses, page-table frames,
  PDE/PTE indices, and the user image limit.
- `kernel/mmio.h` overlays `struct serial_registers`, `struct timer_registers`,
  and `struct irq_router` on the fixed MMIO windows.
- `kernel/syscall.h` owns the scratch-page layout and vector numbers.
- `kernel/trap.h` names the trap-frame words and user-mode flag.
- `kernel/errno.h` names the negative errno values used by syscalls and process
  management.

The BIOS and assembly stubs mirror the same names with `.equ` directives where
the assembler cannot include C headers. The OS C compiler currently accepts only
literal array bounds, so fixed-size arrays use small typedefs with the named
limit in a comment (for example, `process_name_t[32]`).

## Confirmed decisions

- KR32OS targets KR32 only; host compilers never build its sources.
- Boot path: a BIOS-compatible kernel image at fixed physical entry `0x0002_0000`.
- Language split: minimal assembly boot/trap stubs, C kernel and services.
- First runnable milestone: console output, keyboard input, and a small kernel shell.
- Build environment: folder-local scripts plus a CMake orchestrator that invokes
  the KR32 toolchain; `kr32os` is not added to the root CMake build.
- Input: the GUI keyboard feeds a guest console input queue.
- Initial shell: kernel built-ins first; user mode follows once boot is stable.
- Build coverage: Windows PowerShell and WSL Bash wrappers around the same
  orchestration.
- Flat image: `kr32 ld --flat --base 0x00020000` emits the raw kernel image.
- BIOS: lives in `kr32os/bios/`; assembly reset/privileged stubs with optional
  freestanding C helpers for page tables, BootInfo, and diagnostics.
- Kernel heap: bump allocator first; introduce a free-list only when allocation
  patterns justify it.
- Kernel/user ABI: stable numbered syscall vectors with explicit user-pointer
  validation and copy-in/copy-out helpers.
- Multicore: single core through M4; bring up secondary cores after the
  scheduler and user-mode path are stable.
- Console: a new MMIO serial/tty device at `0xFFF05000`; the existing console
  remains a host/debug output path.
- UI input: hover the console panel to send text keys, Enter, Backspace, and
  Ctrl-V paste; function keys stay as debugger shortcuts.
- First user process: flat image at `0x00400000`, stack top `0x07F00000` with a
  guard page at `0x07E00000`; the first program is an embedded initrd blob.
- Physical memory: bitmap page allocator, one bit per 4 KiB page.
- Early exceptions: panic with vector/PC/FAR/error diagnostics, then halt.
- User runtime: OS-specific `crt0` and syscall wrappers on vectors `64+`; no
  host ABI dependency.
- Syscall errors: negative errno in `R0`.
- Process model: kernel shell plus one init user test in M4; fork/exec later.
- Initrd: simple header-table archive embedded in the kernel image.
- Timer: 100 Hz tick with a 10 ms scheduling quantum.
- User image limit: 16 MiB; heap grows up to the stack guard.
- Filesystem: read-only initrd filesystem first; writable block filesystem later.
- Initial syscalls: exit, write, read, yield, getpid, brk.
- Block device: add an LBA register at offset `0x18`; 512-byte sectors.
- Free-list allocator: introduced when process teardown or fork first needs to
  reclaim kernel memory, not before.
- Secondary cores: parked by the BIOS and released by the boot core via IPI.
- Process exit: zombie state until reaped; the kernel reaps init.
- Block I/O: immediate completion first, with the IRQ/status path reserved.
- Shell commands: `help`, `echo`, `clear`, `mem`, `ps`.
- Reaping: `waitpid(pid, status_ptr, options)`.
- Context switch: all GPRs, FPRs, TSP, PC, FLAGS, and SP.
- Filesystem inodes: fixed table with direct and single-indirect block pointers.
- Scheduler: circular doubly linked run queue.
- User faults: kill the faulting process; kernel faults still panic.
- Process table: fixed 256 entries, PID 1 reserved for init, monotonic PID reuse.
- Exit status: POSIX-like wait-status encoding (high byte exit code, low bits
  fault/signal reason).
- Writable filesystem superblock: fixed 4 KiB block with `K32F` magic.
- Shell file commands: `ls`, `cat`, and `run`.
- Initrd: `K32I` magic, version 1, 64 entries, 32-byte names.
- Process table full: return `-EAGAIN`; do not disturb existing processes.
- `run` ABI: `argc`/`argv` on the user stack; fds 0/1/2 connected to serial.
- Process creation: spawn now; POSIX-like `fork`/`exec` later.
- First fd table: only stdin/stdout/stderr (0/1/2); file descriptors follow the VFS.
- Shell quoting: double quotes, backslash escapes, and whitespace splitting.
- Paths: flat initrd names first; a slash-separated tree with the block FS.
- Block I/O: immediate through M4; IRQ completion in M5.
- User pages: W^X segments; code RX, read-only data R, data/heap/stack RW and
  non-executable.
- Init: M4 init test exits; the kernel shell remains.
- Framebuffer: kernel-only initially; no direct user mapping.
- Fork/exec: eager page copy plus initrd `exec`; copy-on-write later.
- Directory entries: fixed 32-byte records with inode number and 28-byte name.
- Shell: kernel shell remains through M5; user shell follows stable fork/exec and fds.
- Framebuffer: kernel text console plus screen fill initially.
- Idle cores: interruptible idle loop; timer/serial IRQs wake the core.


## Boot contract (initial assumption)

The current reference BIOS enters the kernel at `0x0002_0000` with:

- paging enabled and an identity map for RAM, ROM, and MMIO;
- `PTBR = 0x0004_0000`, `VTBR = 0x0005_0000`;
- `SP = KSP = 0x0010_0000`;
- kernel mode, interrupt-enable state as supplied by the BIOS;
- console output at MMIO `0xFFF0_2000`.

Initial low-memory layout:

```text
0x0002_0000  kernel image
0x0003_F000  BootInfo page
0x0004_0000  page directory and page tables
0x0005_0000  vector table
0x0006_0000  stack guard page (not present)
0x0006_1000  usable stack
0x0010_0000  SP / KSP, stack grows down
```

The low 4 MiB is identity-mapped, so the kernel's linked addresses are valid
both before and after paging is enabled. The guard page protects the vector
table and page tables from ordinary stack growth.

Initial high-memory layout:

```text
0x0801_0000..0xFBFF_FFFF  unbacked gap
0xFC00_0000..0xFDFF_FFFF  framebuffer aperture (32 MiB)
0xFE00_0000..0xFFEF_FFFF  unbacked gap
0xFFF0_0000..0xFFFE_FFFF  MMIO window
0xFFFF_0000..0xFFFF_FFFF  ROM
```

The framebuffer uses a 1920x1080 RGBA surface with a 7680-byte stride. Front and
back buffers are at offsets `0x000000` and `0x7E9000`. Control registers live at
`0xFFF0_4000`. ROM and MMIO are mapped supervisor-only (`U = 0`); ROM is
read-only and executable, while MMIO is writable and non-executable.

The framebuffer aperture needs eight page tables (`PDE 0x3F0..0x3F7`) in the
low page-table region. Those mappings are identical for every process and can
share the same page tables. Framebuffer PTEs are writable, non-executable, and
supervisor-only initially. User processes do not map the framebuffer; the kernel
draws directly or exposes a narrow drawing API. A trusted user-mode GUI process
can receive a mapping later.

The first kernel framebuffer API is a text console plus a full-screen fill:

```text
fb_fill(color)
fb_console_putc(ch)
fb_console_clear()
```

The text console uses a small fixed bitmap font and a configurable foreground
and background color. Scrolling and a cursor are part of the console layer, not
the raw framebuffer.

The loader writes BootInfo at the fixed physical page `0x0003_F000` and passes
its address in `R2`. The BIOS validates the magic, version, struct size, and
ranges before using any field.

The structure carries:

```c
uint32_t magic;              /* "K32B" */
uint32_t version;
uint32_t size;               /* sizeof(BootInfo) */
uint32_t ram_base;
uint32_t ram_size;
uint32_t kernel_base;
uint32_t kernel_size;
uint32_t kernel_entry;
uint32_t framebuffer_base;   /* 0xFC000000 */
uint32_t framebuffer_size;   /* 0x02000000 */
uint32_t framebuffer_stride; /* 7680 */
uint32_t framebuffer_format; /* 1 = RGBA8888 */
uint32_t initrd_base;
uint32_t initrd_size;
```

The BIOS rejects an unknown magic/version and refuses to boot if the kernel or
framebuffer ranges fall outside the installed memory or MMIO map.

For M1-M3 there is one kernel address space. When user processes arrive, the
kernel low-memory identity window must either be replicated in every process
page table with supervisor-only permissions, or the trap path must switch to a
separate kernel `PTBR`. Replicating the supervisor mappings is the simpler first
option.

The kernel installs its own vector table entries before relying on faults or
interrupts. The first milestone keeps the BIOS identity map; a later milestone
owns page-table construction.

## BIOS structure

The BIOS is a separate ROM component, versioned with the OS because it defines
the boot ABI:

```text
bios/entry.S    reset vector, privileged stubs (WRCR/IRET), low-stack setup
bios/bios.c     optional freestanding helpers: BootInfo, page tables, diagnostics
bios/README.md  build contract and ROM constraints
```

Assembly owns the reset path and anything that executes privileged instructions.
C is introduced only where it reduces complexity, starting with page-table and
BootInfo policy rather than the first boot stub.

BIOS constraints:

- The complete image must fit in the 64 KiB ROM at `0xFFFF_0000`.
- No host libc or standard `_start`; only freestanding C is allowed.
- Writable `.data`/`.bss` cannot live in ROM. BIOS state must be on the stack or
  in an explicitly reserved RAM block.
- `WRCR` and `IRET` remain assembly stubs even when the surrounding logic is C.
- The BIOS must move to a low mapped stack before enabling paging.

## Proposed layers

```text
bios/entry.S        reset vector, privileged stubs, final IRET
bios/bios.c         optional freestanding BootInfo/page-table helpers
boot/entry.S        kernel entry after the BIOS handoff
kernel/main.c       banner, subsystem init, shell loop
kernel/console.c    polled console output/input
kernel/traps.c      vector table, exception reports
kernel/shell.c      built-in commands
include/            shared headers and MMIO definitions
build/              generated artifacts (ignored)
```

## Kernel heap

The first allocator is a monotonic bump allocator over the RAM reported by
BootInfo:

```text
heap_start = first page after kernel .bss and reserved boot structures
heap_end   = RAM end minus the kernel stack guard region
```

`kmalloc` aligns the cursor and returns `null` when the region is exhausted.
`kfree` is a no-op initially. The free-list allocator with boundary tags and
coalescing is introduced when process teardown or fork first needs to reclaim
kernel memory, not during the initial boot/shell milestones. The allocator
replacement stays behind the same `kmalloc`/`kfree` API.

## Syscall ABI

KR32 software vectors `32..255` are available through the vector table. The host
already uses `32..50` for its debug services, so the KR32OS ABI reserves a
separate range starting at vector `64` (`0x40`):

```text
vector 64..95   KR32OS kernel syscalls
R0..R3          arguments
R0              return value / negative error
```

The exact syscall numbers are assigned in a table and remain stable after the
first user program depends on them. The initial table is:

| Vector | Call | Arguments | Returns |
| --- | --- | --- | --- |
| 64 | `exit` | status | does not return |
| 65 | `write` | fd, buffer, length | bytes written |
| 66 | `read` | fd, buffer, length | bytes read |
| 67 | `yield` | none | 0 |
| 68 | `getpid` | none | process id |
| 69 | `brk` | new break | resulting break or `-ENOMEM` |

Errors are returned as negative errno values in `R0`; success is non-negative.
The initial error set follows POSIX numbering:

| Name | Value |
| --- | --- |
| `EBADF` | 9 |
| `ENOMEM` | 12 |
| `EFAULT` | 14 |
| `EINVAL` | 22 |
| `ENOSYS` | 38 |

Every unused software vector is initialized to a `BRK` stub, so an undefined or
unimplemented syscall raises a contained user breakpoint instead of jumping to
an unknown address. Implemented dispatch entries can still return `-ENOSYS` for
unknown internal operations.

Every user pointer is range-checked against the current user address space
before use. Kernel copy-in/copy-out helpers validate the entire range, handle
page boundaries, and never trust a user pointer directly.

The host's `32..50` vectors remain debug/host services and must not become part
of the OS ABI. The OS console path should use the MMIO console once devices are
attached to an OS boot; the host console is only a temporary bring-up aid.

## Serial console

The OS-facing tty is a separate MMIO device at `0xFFF0_5000`:

| Offset | Register |
| --- | --- |
| `0x00` | data: read consumes RX, write transmits TX |
| `0x04` | status: RX-ready, TX-ready, overrun |
| `0x08` | peek: next RX byte without consuming it; `0xFFFFFFFF` when empty |

The RX FIFO is 4 KiB. When full, new bytes are dropped and the overrun bit is
reported. The first shell polls status/peek; an RX interrupt comes later.

The device is raw: the shell performs line editing and echo. Ctrl-C, Ctrl-D, and
other control combinations are delivered as their byte values. Ctrl-V is the
one reserved UI shortcut and pastes clipboard text, normalized to LF.

The existing console at `0xFFF0_2000` remains a host/debug output path. The UI
console panel displays serial output and captures input while the pointer is
over the console panel. Printable keys, Enter, Backspace, and Ctrl-V are sent to
the guest; F-keys remain debugger shortcuts.

## User address space

```text
0x0000_0000..0x003F_FFFF  kernel low-memory window, user-inaccessible
0x0040_0000..            user image / heap
0x07E0_0000              user stack guard page
0x07E0_1000..0x07EF_FFFF  user stack (grows down from 0x07F0_0000)
0x07F0_0000              initial user stack top
```

The first user image is a flat binary copied from an embedded initrd blob in the
kernel image. It contains no relocations or dynamic loader. Code frames are
allocated from the physical bitmap, copied through a temporary writable user
mapping, then made RX before entry. The user heap starts after the image and
must stop before the stack guard. The image is capped at 16 MiB.

User mappings follow W^X:

```text
code             RX
read-only data   R
data, heap       RW, NX
stack            RW, NX
```

The kernel low-memory window stays supervisor-only. User code cannot execute
from the heap or stack.

The physical page allocator is a bitmap with one bit per 4 KiB page. Low 4 MiB
(kernel, page tables, BootInfo, vectors, MMIO/ROM windows, and heap) and the
identity-mapped user-stack window are reserved before any user code frame is
allocated.

The user runtime is OS-specific: a small `crt0` calls `main`, and syscall
wrappers use vectors `64+`. Host vectors `32..50` are not part of this runtime.
The first `crt0` only needs `exit`, `write`, `read`, `yield`, `getpid`, and
`brk`. `sys_write` treats its buffer as a user virtual address and translates
each page through `user_virt_to_phys`; it never dereferences a raw user pointer
as though it were a kernel address.

The initrd uses a simple header table:

```c
uint32_t magic;              /* "K32I" = 0x4932_334B */
uint32_t version;            /* 1 */
uint32_t count;              /* at most 64 */
struct {
    char name[32];
    uint32_t offset;
    uint32_t size;
    uint32_t entry;
} entries[];
```

The archive supports at most 64 entries and 32-byte filenames.

The initial filesystem is a read-only initrd lookup layer over this archive.
The archive is generated by the build and emitted as a `.byte` assembly blob
linked into kernel `.rodata`; no assembler `.incbin` support is required.

A writable block filesystem follows after the block driver is stable. Its
planned layout is 4 KiB blocks with:

```text
block 0       superblock
block 1..N    free bitmap
next          inode table
remaining     file and directory data blocks
```

The 4 KiB superblock contains:

```c
uint32_t magic;              /* "K32F" */
uint32_t version;            /* 1 */
uint32_t block_size;         /* 4096 */
uint32_t block_count;
uint32_t inode_count;
uint32_t root_inode;
uint32_t bitmap_start;
uint32_t inode_table_start;
uint32_t data_start;
```

The magic is the little-endian value `0x4632_334B`. Version 1 uses 64-byte
inodes and no double-indirect blocks.

The filesystem remains separate from the initrd lookup layer so user programs
can move from read-only initrd files to persistent storage without changing the
VFS API.

Inodes use a fixed table with 64-byte entries. Each inode carries mode, link
count, size, several direct block pointers, one single-indirect pointer, and
timestamps. Direct pointers cover small files without an indirect block; the
single-indirect block covers the next stage of growth. Double-indirect blocks
are deliberately deferred.

Directory data is an array of fixed 32-byte entries:

```c
uint32_t inode;
char name[28];
```

An inode number of zero marks an empty entry. Names are NUL-terminated within
the 28-byte field. Fixed entries make scanning and validation simple.

The block device is extended with an LBA register at offset `0x18`, addressed in
512-byte sectors. A filesystem 4 KiB block is eight consecutive sectors. The
existing DMA address/count interface remains. Requests complete immediately
through M4; IRQ completion arrives in M5 once the scheduler supports blocking
and waking drivers.

A user-mode fault terminates only the faulting process and stores a fault status
for reaping. Kernel-mode faults print the vector, PC, FAR, and error code to the
serial console and halt. The user fault path does not attempt to retry the
faulting instruction except where demand paging explicitly requires it later.

`exit` marks a process as a zombie and stores its exit status. A parent reaps it;
init is reaped by the kernel. Zombies hold their process-table entry and exit
status but release their address space and physical pages immediately. This
leaves room for a future `wait`/`waitpid` syscall without changing the exit
path.

The reaping ABI is `waitpid(pid, status_ptr, options)`. `pid` selects a child or
uses a reserved "any child" value; `status_ptr` receives the exit status; and
`options` is reserved zero for now. The kernel reaps init when no parent exists.

Status uses POSIX-like encoding:

```text
bits 0..6    termination reason: 0 normal, nonzero fault/signal code
bit 7        core/fatal marker, currently zero
bits 8..15   normal exit code
```

A normal `exit(3)` produces `3 << 8`; a fault records a nonzero low reason so
the shell can distinguish a crash from a requested exit.

The process table has 256 fixed entries. PID 1 is reserved for init; PIDs
increase monotonically and are reused only after a slot is reaped. A fixed table
keeps lookup and memory bounded while the scheduler is simple. When the table is
full, creation returns `-EAGAIN`; the shell reports the condition and no existing
process is disturbed.

The M4 init process is a test program: it runs in user mode, prints a result, and
exits. The kernel reaps it and the kernel shell continues. A user-mode shell can
replace this later, but the kernel shell remains the recovery path. The switch is
deferred until M5's fork/exec, descriptor table, and process lifecycle are
stable.

When `fork` arrives, it eagerly copies user pages into a new address space.
`exec` loads a new flat image from the initrd into that address space. COW is a
later optimization behind the same process API.

The first kernel shell commands are:

```text
help     list commands
echo     print its arguments
clear    clear the serial console
mem      physical page and heap statistics
ps       process table
```

Once the initrd is available, add:

```text
ls       list initrd files
cat      print a file
run      load and start a user program from the initrd
```

`run` builds a conventional `argc`/`argv` block on the new user stack and opens
file descriptors 0, 1, and 2 onto the serial console. M4 starts programs
directly; POSIX-like `fork` and `exec` are added after address-space copying and
program loading are stable.

The shell splits arguments on whitespace, supports double quotes, and treats a
backslash as an escape. There is no globbing, variable expansion, or pipeline
syntax initially. Single quotes can be added later if needed.

The initial per-process file-descriptor table holds only 0, 1, and 2, all
connected to the serial console. Initrd files are accessed through the kernel
shell's `ls`/`cat` commands until the VFS and a real descriptor table exist.
The block filesystem introduces a slash-separated path tree; the initrd namespace
stays flat until then.

## Timer and scheduling quantum

The timer runs at 100 Hz. Each tick decrements the current quantum; when it
reaches zero the kernel schedules the next runnable thread. The initial quantum
is 10 ms, or one timer tick. Timer IRQ handling and context switching arrive in
M5; M3 only establishes the vector and timer device path.

A context switch saves and restores:

- all 16 general-purpose registers;
- all 16 floating-point registers;
- `TSP`;
- `PC`, `FLAGS`, and `SP`.

Lazy floating-point state is not attempted in the first scheduler. This keeps
user FP and transform-stack state correct even though switches are slower.

The runnable queue is a circular doubly linked list. The scheduler takes the
current thread off the head, runs it for one quantum, and appends it to the
tail. There is one runnable thread per core in the first multicore milestone.

When no thread is runnable, the core enters an interruptible idle loop with
interrupts enabled. Timer and serial IRQs wake it. KR32 `HALT` stops a core
permanently, so it is not used for idle.

## Multicore policy

M0–M4 run on the boot core only. Secondary cores remain parked until the
scheduler, trap path, and one user process are stable. During M5 bring-up the
BIOS/loader places each secondary core in a parked spin loop with a per-core
boot slot. The boot core finishes initialization, writes the startup entry
point, and releases each core with an IPI. Multicore bring-up then adds:

- per-core stacks and kernel state;
- a parked-core startup path;
- IPIs for scheduling and TLB/page-table coordination;
- per-core idle loops.

There is no core oversubscription in the first multicore milestone: one runnable
thread per core at a time.

## Proposed milestones

1. **M0 — scaffold:** build scripts, linker layout, boot stub, halt on entry.
2. **M1 — boot banner:** kernel entry at `0x0002_0000`, console output, clean halt.
3. **M2 — console + shell:** serial tty output, polled RX, prompt, `help`,
   `echo`, `clear`, `mem`.
4. **M3 — faults + timer:** vector table, exception diagnostics, timer IRQ.
5. **M4 — memory + processes:** bitmap page allocator, kernel heap, embedded
   initrd, first user-mode image.
6. **M5 — scheduler + multicore:** context switching, per-core state, IPIs, secondary cores.

## M1 implementation sketch

- `entry.S` sets a kernel stack, clears `.bss`, and calls `kmain`.
- `kmain` prints a banner through the console abstraction and halts cleanly.
- Console output initially uses the existing host `putchar` path (syscall 33)
  behind `console_putc`; direct MMIO `0xFFF0_2000` is the next fallback.
- Console input starts as a guest input queue in the debug session. A `getchar`
  request with no queued byte pauses the run loop and waits for a GUI key, so
  the shell does not busy-spin.
- Shell built-ins run in kernel mode for M2.

## Build pipeline (proposal)

```text
kr32os/build.ps1          Windows entry point
kr32os/build.sh           WSL entry point
kr32os/CMakeLists.txt     project(KR32OS NONE), custom commands
kr32os/bios/entry.S       ROM reset path and privileged stubs
kr32os/bios/bios.c        optional freestanding BIOS helpers
kr32os/boot/entry.S       kernel entry
kr32os/kernel/*.c         compiled with kr32 cc -S
```

The orchestrator:

1. Compiles C sources to KR32 assembly with `kr32 cc -S`.
2. Assembles each generated `.s` and `entry.S` with `kr32 as --object`.
3. Links the kernel objects with `kr32 ld --flat --base 0x00020000`.
4. Builds `bios.bin` from `bios/entry.S` plus any `bios/*.s` at `0xFFFF0000`.
5. Runs with `kr32 os kernel.bin --bios bios.bin`.

## Booting from the CLI

`kr32 os` boots the default kernel and BIOS. Explicit images are also allowed:

```text
kr32 os
kr32 os path/to/kernel.bin --bios path/to/bios.bin
kr32 gui --os
```

The default kernel is resolved from `KR32OS_BIN`, then `kr32os.bin` next to the
executable, then `kr32os/build/<platform>/kernel.k32` or `kernel.bin` in the
current directory, then an installed `share/kr32` directory, and finally the
embedded kernel/BIOS pair compiled into `kr32`. The BIOS defaults to
`KR32OS_BIOS`, the kernel's sibling `bios.k32`/`bios.bin`, or the same
locations.

## Follow-up design questions

- What are the exact inode fields and permission bits?
- When should eager fork copying become copy-on-write?
- How should `exec` pass arguments and environment to the new image?
- Which bitmap font and glyph size should the framebuffer console use?
- When should the user shell become the default shell?
