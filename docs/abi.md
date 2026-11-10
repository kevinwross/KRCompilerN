# ABI — KRCompiler C/C++ ABI

This document specifies the application binary interface for KRCompiler.
It is a work in progress; the authoritative decisions are in `plan.md`
§10 (ABI specification) and Appendix A (D-010, D-011, D-012).

## Status

- **Specified:** plan.md §10 (data models, calling conventions, name mangling,
  vtables/RTTI/exceptions, varargs, TLS, guards, C interop, ABI versioning).
- **Implemented:** Not yet. No target descriptor ABI tables exist in `src/`.
- **Frozen:** Not yet frozen. ABI changes require an ADR under `docs/decisions/`
  plus an Appendix A entry (plan.md §18.1).

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| KR32 data model (ILP32) | §10.1 |
| KR16 data model (IP16 profile) | §10.1 |
| KR64 data model (LP64 placeholder) | §10.1 |
| KR32 calling convention | §10.2 |
| KR16 calling convention (to be frozen before M7) | §10.2 |
| Name mangling (Itanium-like) | §10.3 |
| Vtables, RTTI, exceptions | §10.4 |
| Varargs | §10.5 |
| TLS and thread control block | §10.6 |
| Guards and static initialization | §10.7 |
| C interoperability and attributes | §10.8 |
| ABI versioning | §10.9 |

## Related

- `docs/abi-kr16.md` — KR16 ABI appendix (required before M7, plan.md §10.2)
- `docs/attributes.md` — OS attributes (`interrupt`, `naked`, `noreturn`, etc.)
- `docs/targets/kr32.md`, `docs/targets/kr16.md` — target profiles
- `plan.md` Appendix A — D-010, D-011, D-012
