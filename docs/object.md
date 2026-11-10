# Object Format — KRO

This document specifies the KRCompiler object container format (KRO).
It is a work in progress; the authoritative specification is in `plan.md`
§11 (Object format) and Appendix A (D-013, D-014).

## Status

- **Specified:** plan.md §11 (header, sections, symbols, relocations,
  validation/determinism, executable images).
- **Implemented:** Not yet. No object reader/writer exists in `src/linker/`.
- **Frozen:** Not yet frozen. Object format changes require an ADR under
  `docs/decisions/` plus an Appendix A entry (plan.md §18.1).

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| Header (magic `KRO\0`, version, target ID, ABI version, build ID, CRC32) | §11.1 |
| Sections (types, flags, 64-bit addresses/offsets) | §11.2 |
| Symbols (name, value, size, binding, type, visibility) | §11.3 |
| Relocations (absolute, PC-relative, hi/lo, GOT/PLT reserved) | §11.4 |
| Validation and determinism (no timestamps, sorted symbols, build-root remap) | §11.5 |
| Executable images (per-target image writers) | §11.6 |

## Related

- `docs/symbols.md` — symbol/debug format (KDBG)
- `src/linker/` — object reader/writer, `krld`, image writers
- `plan.md` Appendix A — D-013, D-014, D-015
