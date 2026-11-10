# Runtime — Common Runtime and Libraries

This document specifies the KRCompiler runtime and standard library.
It is a work in progress; the authoritative specification is in `plan.md`
§14 (Runtime and libraries) and Appendix A (A8, A8b through A8g).

## Status

- **Specified:** plan.md §14 (architecture, startup, C library surface,
  C++ runtime, standard library ladder, host services, soft-float/64-bit
  helpers, distribution and bootstrap).
- **Implemented:** Not yet. No runtime source exists in `src/runtime/`.
- **Frozen:** Not yet frozen.

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| Architecture (`src/runtime/common/`, `src/runtime/<target>/`, sysroot) | §14.1 |
| Startup (`_start`/crt0, `.init_array`, `.fini_array`, `main`, `exit`) | §14.2 |
| C library surface (headers and functions for v1.0) | §14.3 |
| C++ runtime (`operator new`/`delete`, `__cxa_*`, unwinder, exceptions v1.1) | §14.4 |
| Standard library ladder (v1.0 freestanding → v1.3 iostream/map) | §14.5 |
| Host services (common host-service ABI, per-VM shims) | §14.6 |
| Soft-float and 64-bit helpers | §14.7 |
| Distribution and bootstrap | §14.8 |

## Related

- `src/runtime/` — common runtime, HAL, per-target stubs, sysroot
- `sysroot/` — target headers and prebuilt libraries
- `plan.md` Appendix A — A8 through A8g
