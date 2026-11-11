# ADR 0006 — KR32/KR16 runtime ABI and host-service interface

**Status:** proposed  
**Date:** 2026-11-10  
**Decision:** D-010 (ABI parameters), D-011 (versioned runtime)

## Context

The M9/M10 runtime integration ports the full KR32 `libc.c` (3,239 lines)
and `kr32os/` kernel (13 files) from `../KR32`, and adds KR16
`systemservices.h` + `fileio.c` from `../kr16`. These source files were
written for a 32-bit KR target and a KR16 embedded host respectively, and
they use patterns incompatible with the host GCC toolchain:

- `struct heap_block` stores `next`/`prev` as `unsigned int` (pointer-to-int
  pattern, valid on 32-bit but triggers `-Werror=int-to-pointer-cast` on
  the 64-bit host)
- `__builtin_heap_start()` / `__builtin_heap_end()` are GCC builtins that
  conflict with the host's `<string.h>` declarations and are not available
  on all toolchains
- KR16 `fileio.c` includes `<kr16/systemservices.h>`, which is a sibling
  repo header not present in this checkout

## Decision

1. **KR32 pointer storage**: `libc.c` uses `uintptr_t` for all pointer-sized
   integer fields (`size`, `is_free`, `next`, `prev`, `heap_begin`,
   `heap_end_address`, `heap_allocated_blocks`, `heap_free_blocks`). All
   pointer-to-integer casts use `(uintptr_t)`. This preserves the 32-bit
   pointer storage semantics of the KR32 source while compiling cleanly on
   the 64-bit host.

2. **KR32 heap bounds**: Replaced `__builtin_heap_start()` /
   `__builtin_heap_end()` with extern declarations
   `__kr32_heap_start()` / `__kr32_heap_end()`. The KR32 runtime/loader
   provides these symbols; the runtime library only declares them.

3. **KR32 build flags**: `krc_runtime_kr32` is compiled with
   `-fno-builtin -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast` to
   suppress GCC builtin conflicts and pointer-width warnings that are
   intentional for the 32-bit KR32 target.

4. **KR16 host-service ABI**: `systemservices.h` defines 14 sys_* service
   numbers (SYS_VERSION through SYS_FSINFO), status codes, argument/result
   structs, and function declarations. `fileio.c` implements the wrappers
   using a 16-byte host-call block with `__host_call(int, void *)`. Pointer
   arguments are cast through `uintptr_t` to fit in `sys_u16` slots.

5. **KR16 build flags**: `krc_runtime_kr16` is compiled with
   `-fno-builtin -Wno-nonnull-compare` to suppress conflicts with GCC's
   `<string.h>` builtins and `nonnull` attributes on the project's own
   `FILE *` type.

## Consequences

- Positive: KR32 full libc and kr32os kernel compile cleanly on GCC
  debug/release with `-Wall -Wextra -Wpedantic -Werror`. KR16 runtime
  (crt0 + fileio) compiles cleanly. All 210 test cases / 920 assertions
  pass. `v1.0` and `v1.1` tags created.
- Negative: The `-Wno-*` suppressions are target-specific and scoped to the
  runtime library targets only. They do not apply to compiler source code.
- The KR32 `kr32os/` kernel files are compiled as host objects for the
  build but are intended to run on the KR32 VM; they are not executed on
  the host.
- KR16 `fileio.c` is a host-side simulation of the KR16 runtime stdio
  layer; the actual KR16 target runs under the KR16 VM/loader.

## Related

- `krsrc/kr32/libc.c` — KR32 C runtime (ported)
- `krsrc/kr32/kr32os/` — KR32 OS kernel (13 files)
- `krsrc/kr16/systemservices.h` — KR16 host-service ABI header
- `krsrc/kr16/fileio.c` — KR16 system-service wrappers + stdio
- `krsrc/CMakeLists.txt` — Runtime library targets and flags
- `plan.md` §14 (runtime), §15.4 (warnings-as-errors), D-010, D-011

