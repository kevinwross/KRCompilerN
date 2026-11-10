# Versions — KRCompiler Compatibility Matrix

This document maintains the compatibility matrix for KRCompiler.
It is a work in progress; the authoritative versioning rules are in
`plan.md` §15.6 (Determinism and versioning) and Appendix A (D-011, D-035).

## Status

- **Specified:** plan.md §15.6 (separate versions: compiler, language mode,
  ABI, object format, debug format, target descriptor; compatibility matrix;
  build IDs and source hashes).
- **Implemented:** Not yet. No versioned artifacts exist yet.

## Version components

| Component | Specified in | Current value |
|---|---|---|
| Compiler version | `CMakeLists.txt` `project(VERSION …)` | 0.1.0 |
| Language mode | plan.md §6 | C11 / C++11 default for `.cpp` |
| ABI version | plan.md §10.9 (D-011) | not yet assigned |
| Object format (KRO) | plan.md §11 (D-013) | not yet assigned |
| Debug format (KDBG) | plan.md §13 (D-016) | not yet assigned |
| Target descriptor schema | plan.md §4.2 (D-007) | not yet assigned |

## Related

- `plan.md` §15.6 — Determinism and versioning
- `plan.md` Appendix A — D-011 (ABI stability), D-035 (Determinism)
- `CMakeLists.txt` — `project(krc VERSION …)`
