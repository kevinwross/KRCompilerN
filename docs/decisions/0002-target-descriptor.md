# ADR 0002 — Target descriptor schema v1

**Status:** proposed  
**Date:** 2026-11-09  
**Decision:** D-007, plan.md §4.2

## Context

KRCompiler needs per-target descriptions covering data model, ABI, encoding,
layout, and capabilities (D-003, D-007). The v1 choice is C++ `constexpr`
tables compiled into `krc`, not a runtime plugin or data-file format
(plan.md §4.2, open question 3).

## Decision

- Target descriptors are C++ `constexpr` tables plus small hooks, compiled
  into `krc`.
- Table sections: identity, data model, registers, instructions, ABI,
  object/image, capabilities, feature macros, constraints (plan.md §4.2).
- One descriptor per target: `kr32`, `kr16`, `kr64` (placeholder).
- A data-file descriptor format MAY be added later, but C++ tables are the
  v1 contract.

## Consequences

- Positive: no external dependency, compile-time validation, target-specific
  choices isolated from the front end.
- Negative: adding a target requires recompiling `krc`; not a runtime plugin.

## Related

- `docs/targets/kr32.md`, `docs/targets/kr16.md`, `docs/targets/kr64.md`
- `src/backend/targets/kr32/`, `src/backend/targets/kr16/` (not yet created)
- `plan.md` §4.2, Appendix A (D-007)
