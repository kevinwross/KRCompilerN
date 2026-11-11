# Runtime — Common Runtime and Libraries

This document specifies the KRCompiler runtime and standard library.
It is a work in progress; the authoritative specification is in `plan.md`
§14 (Runtime and libraries) and Appendix A (A8, A8b through A8g).

## Status

- **Specified:** plan.md §14 (architecture, startup, C library surface,
  C++ runtime, standard library ladder, host services, soft-float/64-bit
  helpers, distribution and bootstrap).
- **Implemented:** Partial runtime in `krsrc/`:
  - `krsrc/common/hal.h` — HAL interface (plan.md §14.1, §14.6)
  - `krsrc/common/allocator.c` — Coalescing free-list malloc/free
  - `krsrc/common/ctype.c` — isalnum, isalpha, isascii, isblank,
    iscntrl, isdigit, isgraph, islower, isprint, ispunct, isspace,
    isupper, isxdigit, tolower, toupper
  - `krsrc/common/errno.c` — Global errno variable + __krc_assert_fail
  - `krsrc/common/fileio.c` — fopen, fclose, fread, fwrite, fputc,
    fputs, fgetc, fprintf on top of HAL
  - `krsrc/common/math.c` — llabs, isqrt, ipow, idiv_round, igcd
  - `krsrc/common/stdio.c` — printf, vprintf, puts, putchar
  - `krsrc/common/string.c` — memcpy, memmove, memset, memcmp,
    strlen, strcpy, strncpy, strcat, strncat, strcmp, strncmp, strchr,
    strrchr, strstr, strdup
  - `krsrc/common/stdlib.c` — exit, abort, atoi, atol, strtol,
    abs, labs, qsort, bsearch
  - `krsrc/common/time.c` — time(), clock() via krc_hal_time()
  - `krsrc/common/cxa_runtime.cpp` — Itanium C++ ABI __cxa_* runtime
  - `krsrc/common/unwinder.c` — Frame-pointer-based stack unwinder
  - `krsrc/common/unwinder.h` — Unwinder interface header
  - `krsrc/common/host_abi.c` — Host ABI shims (standalone, not linked
    into krc_runtime_common to avoid symbol conflicts)
  - `krsrc/kr32/crt0.c` — KR32 startup
  - `krsrc/kr16/crt0.c` — KR16 startup
  - `krsrc/CMakeLists.txt` — krc_runtime_common, krc_runtime_kr32,
    krc_runtime_kr16 targets
- **Frozen:** Not yet frozen.

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| Architecture (`krsrc/common/`, `krsrc/<target>/`, sysroot) | §14.1 |
| Startup (`_start`/crt0, `.init_array`, `.fini_array`, `main`, `exit`) | §14.2 |
| C library surface (headers and functions for v1.0) | §14.3 |
| C++ runtime (`operator new`/`delete`, `__cxa_*`, unwinder, exceptions v1.1) | §14.4 |
| Standard library ladder (v1.0 freestanding → v1.3 iostream/map) | §14.5 |
| Host services (common host-service ABI, per-VM shims) | §14.6 |
| Soft-float and 64-bit helpers | §14.7 |
| Distribution and bootstrap | §14.8 |

## Related

- `krsrc/` — common runtime, HAL, per-target stubs, sysroot
- `sysroot/` — target headers and prebuilt libraries
- `plan.md` Appendix A — A8 through A8g

