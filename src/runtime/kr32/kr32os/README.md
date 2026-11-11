# KR32OS

KR32OS is a small operating system for the KR32 virtual machine.

## Scope

- All sources in this directory target the KR32 ISA and KR32 toolchain.
- Host compilers (MSVC, GCC, Clang) must not compile or include these sources.
- The root CMake build intentionally does not add this directory.

## Status

M0-M4 boot path is implemented: BIOS boot and paging, serial shell, exception
reporting, timer IRQ, initrd loading, process table, and a first user-mode init
process. Remaining M4 infrastructure work is listed in
[kr32os-impl.md](kr32os-impl.md). Architecture is recorded in
[DESIGN.md](DESIGN.md).

## Build

Windows:

```text
powershell -ExecutionPolicy Bypass -File kr32os/build.ps1
```

WSL/Linux:

```text
bash kr32os/build.sh
```

Both scripts accept `KR32` through the environment; `build.ps1` also accepts
`-Kr32 <path>`. The default is the host Debug build. Kernel and user C sources
are compiled with `-O1` by default; set `KR32OS_OPT` (or pass `-Opt` to
`build.ps1`) to choose a different optimization level, for example
`-Opt "-O0"` on PowerShell or `KR32OS_OPT=-O0` on Linux.

Windows writes to `kr32os/build/windows`; WSL/Linux writes to
`kr32os/build/linux`. Keeping them separate prevents a Linux test build from
overwriting Windows `.k32` files and their Windows debug source paths. Both
scripts also accept `KR32OS_BUILD_DIR`; `build.ps1` additionally accepts
`-Out <dir>`.

The build emits:

- `bios.bin` / `kernel.bin` — boot images loaded at `0xFFFF_0000` and
  `0x0002_0000`;
- `bios.k32` / `kernel.k32` — container images with debug information;
- `bios.sym` / `kernel.sym` — sibling symbol/debug sidecars for the `.bin`
  images.

## Source debugging

`kr32 gui --os` automatically looks for `kernel.sym` next to `kernel.bin` and
loads it after the raw image. The sidecar contains source lines and symbols and
is CRC-checked against the image. BIOS and entry-assembly line records are
included as well, so stepping through `bios/entry.S` and `boot/entry.S` shows
the assembly source pane.

The `.k32` images are also accepted:

```text
kr32 gui --os kr32os/build/windows/kernel.k32 --bios kr32os/build/windows/bios.k32
```

Run:

```text
kr32 os kr32os/build/windows/kernel.bin --bios kr32os/build/windows/bios.bin
```

The `.k32` pair is accepted by `os` as well:

```text
kr32 os kr32os/build/windows/kernel.k32 --bios kr32os/build/windows/bios.k32
```

For a scripted session:

```text
kr32 os kr32os/build/windows/kernel.bin --bios kr32os/build/windows/bios.bin \
    --serial-input "ls
run init
exit
"
```

## Planned layout

```text
kr32os/
  bios/       reset vector, ROM bootstrap, page tables, firmware stubs
  boot/       kernel entry and BIOS handoff
  kernel/     traps, memory, console, syscalls
  libc/       OS-specific crt0 and syscall wrappers
  user/       shell and test programs
  include/    shared KR32OS headers
  build/      host-side build scripts and generated artifacts (ignored)
```
