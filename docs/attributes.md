# Attributes — OS and Language Attributes

This document specifies the KRCompiler target attributes.
It is a work in progress; the authoritative specification is in `plan.md`
§6.4 (Target extensions and attributes) and Appendix A (D-027).

## Status

- **Specified:** plan.md §6.4 (matrix/transform builtins, atomics, host-service
  syscalls, volatile/MMIO, attributes `interrupt`, `naked`, `noreturn`,
  `section`, `aligned`, custom calling conventions; syntax `[[krc::*]]` and
  `__attribute__((*))`).
- **Implemented:** Not yet. No attribute grammar or handling exists in the
  front end.
- **Frozen:** Not yet frozen. Attribute ABI effects are target-specific and
  require an ADR under `docs/decisions/` if they touch ABI/object/IR.

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| Matrix/transform builtins (KR32) | §6.4 |
| Atomics builtins (`__builtin_ll`, `__builtin_sc`, `__builtin_fence`) | §6.4 |
| Host-service/syscall builtins (`__builtin_kr32_syscall`) | §6.4 |
| Volatile/MMIO access helpers | §6.4 |
| Attributes: `interrupt`, `naked`, `noreturn`, `section`, `aligned`, custom conventions | §6.4 |
| Inline assembly (not in v1.0) | §6.4 |

## Related

- `docs/abi.md` — attribute ABI effects (saved registers, return instructions, stack alignment)
- `src/frontend/` — attribute grammar and Sema handling
- `plan.md` Appendix A — D-027
