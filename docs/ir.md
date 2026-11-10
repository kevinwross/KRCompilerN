# IR — Typed SSA Intermediate Representation

This document specifies the KRCompiler typed SSA IR.
It is a work in progress; the authoritative specification is in `plan.md`
§8 (IR specification) and Appendix A (D-005, D-006).

## Status

- **Specified:** plan.md §8 (module/function model, types, memory and effects,
  control flow and calls, vtables/RTTI, debug information, optimisation levels,
  textual dump, verifier).
- **Implemented:** Not yet. No IR model, builder, verifier, or printer exists
  in `src/ir/`.
- **Frozen:** Not yet frozen. IR changes require an ADR under `docs/decisions/`
  plus an Appendix A entry (plan.md §18.1).

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| Module and function model | §8.2 |
| Types (`void`, `bool`, `i8`/`i16`/`i32`/`i64`, `f32`/`f64`, `ptr`, `array`, `struct`, …) | §8.3 |
| Memory and effects (`load`/`store`, `volatile`, atomics, no strict aliasing) | §8.4 |
| Control flow and calls (direct/indirect calls, `invoke`/`landingpad` v1.1) | §8.5 |
| Vtables and RTTI (Itanium-like layout) | §8.6 |
| Debug information in IR (source locations, variable storage locations) | §8.7 |
| Optimisation levels (`-O0`, `-O1`) | §8.8 |
| Textual dump (`krc --dump-ir`) | §8.9 |
| Verifier (SSA dominance, type consistency, terminators, phi nodes, …) | §8.10 |

## Related

- `src/ir/` — IR model, builder, verifier, printer, passes
- `plan.md` Appendix A — D-005, D-006
