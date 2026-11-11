# Testing — KRCompiler Test Strategy

This document specifies the KRCompiler test strategy.
It is a work in progress; the authoritative specification is in `plan.md`
§5 (Corpus, conformance, and acceptance) and §15.5 (Testing).

## Status

- **Specified:** plan.md §5 (corpus tiers, sysroot, feature extraction from
  doctest strings, corpus migration checklist, conformance strategy,
  acceptance gates G0–G9) and §15.5 (unit, integration, differential,
  conformance, golden, negative, debugger, fuzzing tests).
- **Implemented:** Test suite in `tests/`:
  - `tests/test_sema.cpp` — Sema unit tests (doctest): builtin types, scope
    lookup, type system, class/inheritance/virtual layout, templates, etc.
  - `tests/test_mangle.cpp` — C++ name mangling tests.
  - `tests/test_determinism.cpp` — Determinism tests: KR16 assembler produces
    byte-identical KRO output across double/triple assembly; KR16 linker
    produces byte-identical linked output and valid executable images.
  - `tests/test_target_descriptor.cpp` — Target descriptor tests: 12 test cases
    covering KR32 and KR16 target descriptors — identity, data model, registers,
    capabilities, feature macros, and constraints.
  - `tests/test_kr16_encoding.cpp` — KR16 instruction encoding tests: 8 test
    cases covering R-type, I-type, J-type encodings, RET, NOP, store, branch,
    imm8_fits, register helpers, register classification, allocatable registers.
  - `tests/test_machine_ir.cpp` — Machine IR tests: 16 test cases covering
    MachineOperand (RegUse, RegDef, Imm, StackSlot, Global, Label),
    MachineInstr, MachineBasicBlock, MachineFunction, MachineModule, MachineGlobal,
    and dump_machine_module output.
  - `tests/test_register_allocation.cpp` — Register allocator tests: 6 test
    cases covering basic allocation, spill behavior, unassigned register
    queries, stack slot tracking, and multi-function independence.
  - `tests/test_krdap.cpp` — DAP debugger integration tests: 3 test cases
    verifying the krdap Debug Adapter Protocol server responds correctly to
    initialize, launch, and disconnect requests (debugger parity for M8).
  - `tests/test_frame_layout.cpp` — Frame layout tests: 5 test cases covering
    empty functions, stack slot frame size computation, pointer alignment,
    no-stack-slot functions, and positive-only stack offsets.
  - `tests/test_debug_info.cpp` — Debug info collector tests: 6 test cases
    covering empty modules, function info (start address, size, saved FP/RA,
    CFA rule), variable extraction from stack slots, duplicate suppression,
    multiple functions, and multiple variables.
  - `tests/test_sema.cpp` — Includes 2 new M10 variadic template tests
    ("variadic type parameter pack", "mixed variadic and regular type
    parameters"), 3 new M10 fold expression tests (unary right fold,
    binary left fold, unary left fold AST nodes), 3 new M10 EH/RTTI tests
    (throw expression type checking, typeid of type, typeid of expression),
    and 2 new M10 thread_local tests (storage duration, dynamic initializer),
    and 2 new M10 forward tests (preserves referenced type, deduces from operand),
    and 2 new M10 pair type tests (type compatibility, first/second access).
  - `tests/third_party/doctest/doctest.h` — vendored doctest (the only
    vendored test dependency).
- **Frozen:** Not yet frozen.

## Contents (specified in plan.md)

| Section | plan.md location |
|---|---|
| Corpus inventory and tiers (Tier 0–4) | §5.1, §5.2 |
| Sysroot layout and requirements | §5.3 |
| Feature extraction from doctest strings (`krc_extract_fixtures`) | §5.4 |
| Corpus migration checklist (volatile MMIO, printf, void main, etc.) | §5.5 |
| Conformance strategy (differential, extracted fixtures, curated, golden, negative, fuzzing) | §5.6 |
| Acceptance gates G0–G9 | §5.7 |
| Test types (unit, integration, differential, conformance, golden, negative, debugger, fuzzing) | §15.5 |
| Determinism and versioning | §15.6 |

## Related

- `tests/` — unit, integration, differential, conformance, golden, corpus
- `tests/third_party/doctest/` — vendored doctest (the only vendored test dependency)
- `src/tools/extract_fixtures` — fixture extractor from doctest strings
- `plan.md` Appendix B — corpus tiers and compile-first list
