# Symbols and Debug Format — KDBG

This document specifies the KRCompiler symbol/debug container format (KDBG).
It is a work in progress; the authoritative specification is in `plan.md`
§13 (Symbol and debug format) and Appendix A (D-016, D-017, D-018, D-019).

## Status

- **Specified:** plan.md §13 (container sections, record model, location
  expressions, debugger feature mapping, sidecar/stripping, versioning,
  shared reader library `libkrcdebug`).
- **Implemented:** Not yet. No debug reader/writer exists in `src/debug/`.
- **Frozen:** Not yet frozen. Symbol/debug format changes require an ADR under
  `docs/decisions/` plus an Appendix A entry (plan.md §18.1).

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| Container sections (`.kdbg_header`, `.kdbg_src`, `.kdbg_line`, etc.) | §13.2 |
| Record model (tag + length + payload, 64-bit canonical addresses) | §13.3 |
| Location expressions (`frame_offset`, `register`, `tls_offset`, etc.) | §13.4 |
| Debugger feature mapping (breakpoints, stack, variables, aggregates, etc.) | §13.5 |
| Sidecar and stripping (`--debug-file`, `--strip`, build ID) | §13.6 |
| Versioning and robustness (CRC32, build ID, source hashes) | §13.7 |
| Shared reader library `libkrcdebug` | §13.8 |

## Related

- `docs/object.md` — object format (KRO)
- `src/debug/` — symbol/debug format reader/writer, dumper, shared reader
- `plan.md` Appendix A — D-016 through D-019
