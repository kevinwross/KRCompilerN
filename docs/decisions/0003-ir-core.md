# ADR 0003 — IR core — typed SSA, memory model

**Status:** proposed  
**Date:** 2026-11-09  
**Decision:** D-005, D-006

## Context

KRCompiler needs an intermediate representation that is target-independent
enough for KR32 and KR16, typed enough for C++ semantics and debug info, and
explicit about memory, `volatile`, atomics, exceptions, and debug locations
(plan.md §8.1).

## Decision

- Typed SSA in memory plus a textual dump (`krc --dump-ir`).
- No stable serialised IR in v1 (D-005).
- IR contents: explicit exceptions/cleanup, volatile/MMIO, atomics, debug
  locations, vtable/RTTI metadata, source-level types (D-006).
- No address spaces/banking or inline/call-graph metadata in v1.
- Types: `void`, `bool`, `i8`/`i16`/`i32`/`i64`, `f32`/`f64`, `ptr`
  (address space 0 only), `array`, `struct`, `union`, `enum`, `function`,
  `member_ptr`, `vector` (reserved).
- Explicit `load`/`store` with type, alignment, `volatile`, and atomic
  ordering; no strict-aliasing assumptions in v1.

## Consequences

- Positive: simple to inspect and teach; retargetable across KR32/KR16;
  explicit memory model suits volatile/MMIO and atomics.
- Negative: no serialised IR means IR cannot be cached or distributed
  separately in v1.

## Related

- `docs/ir.md` — IR specification
- `src/ir/` — IR model, builder, verifier, printer, passes (not yet created)
- `plan.md` §8, Appendix A (D-005, D-006)
