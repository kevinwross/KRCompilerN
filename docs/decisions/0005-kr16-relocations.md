# ADR 0005 — KR16 relocation types

**Status:** proposed  
**Date:** 2026-11-10T17:19:53-08:00  
**Decision:** D-013, D-014, plan.md §11.4

## Context

KR16 uses 16-bit fixed-width instructions (R/I/L/S/B/J formats). The object
format (KRO, plan.md §11) defines a target-specific relocation range starting
at 0x200 for KR16. The KR32 assembler emits `R_KR32_Branch26` (4-byte
instructions with 11-bit immediate), but KR16's 2-byte instructions need
smaller relocation types:

- `R_KR16_Branch10` — 10-bit branch target for KR16 `J`/`JAL` (2-byte
  instruction, immediate at bits [9:0]).
- `R_KR16_Branch8` — 8-bit branch target for KR16 `BEQ`/`BNE` etc. (2-byte
  instruction, immediate at bits [7:0]).
- `R_KR16_Abs16` — 16-bit absolute relocation for KR16 data references.

The linker previously only handled `R_KR32_Branch26`, `R_Abs8`, `R_Abs16`,
`R_Abs32`, `R_Abs64` — all assuming 4-byte KR32 instruction layout. It needed
variable-width relocation handling and KR16-specific relocation types.

## Decision

- Add `R_KR16_Branch10` (0x200), `R_KR16_Branch8` (0x201), `R_KR16_Abs16`
  (0x202) to `RelocType` in `src/asm/object.h`, in the KR16 target-specific
  range (0x200+).
- `Linker::apply_relocations()` now uses `rel.width` to determine patch size
  instead of hardcoding 4 bytes, and adds cases for all three KR16 relocation
  types. `R_KR16_Branch10` patches bits [9:0] of a 2-byte instruction;
  `R_KR16_Branch8` patches bits [7:0]; `R_KR16_Abs16` patches the full 16
  bits little-endian.
- `krld` sets `pointer_width` based on target: 16 for KR16, 64 for KR64,
  32 for KR32 (was hardcoded to 32).
- The KR16 assembler emits `R_KR16_Branch10` for unconditional branches
  (`J`/`JAL`) and `R_KR16_Branch8` for conditional branches (`BEQ`/`BNE`
  etc.), with width fields of 10 and 8 respectively.

## Consequences

- Positive: KR16 object files can be linked and relocated correctly. The
  linker is now target-width-agnostic for relocation patching.
- Negative: KR32 and KR16 use different relocation type values for similar
  concepts (e.g., `R_KR32_Branch26` vs `R_KR16_Branch10`). This is expected
  and correct — target-specific ranges exist for this purpose.
- No D-xxx decision changes: the relocation type ranges were already reserved
  per-target in D-013/D-014; this ADR only fills in the KR16 sub-range.

## Related

- `src/asm/object.h` — `RelocType` enum, new KR16 relocation types
- `src/linker/linker.cpp` — `apply_relocations()`, variable-width patching
- `src/tools/krld/main.cpp` — `pointer_width` based on target
- `src/asm/kr16_assembler.cpp` — emits KR16 relocation types
- `plan.md` §11.4 (relocations), D-013 (object format), D-014 (linker)
