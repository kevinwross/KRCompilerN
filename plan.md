# KRCompiler — Plan and Specification

**Status:** Approved — 2026-11-09 (plan v0.1). Implementation proceeds under §18.2.
**Date:** 2026-11-09
**Owner:** Kevin
**Scope:** A single retargetable C/C++ compiler, intermediate representation, per-target backends, linker, runtime, and symbol/debug format for the KR-family virtual machines.

**Inputs used for this plan**

- The 2026-11-09 planning survey (R/A/L/G decisions).
- `../kr32/docs/cpp-plan.md`, `../kr32/cpp-handoff.md`, `../kr32/docs/abi.md`, `../kr32/docs/container.md`, `../kr32/docs/debug.md`.
- `../kr16/kr16-plan.md`, `../kr16/docs/compiler-opt.md`, `../kr16/docs/decisions/0001-16-bit-long.md`.
- The 2026-11-09 corpus inventory of `../kr32` and `../kr16`.

> Normative terms: **MUST**, **MUST NOT**, **SHOULD**, **MAY**. Decision identifiers (`D-xxx`) are stable. Changing a decision requires an entry in the decision log (Appendix A) and, for architecture or ABI changes, an ADR under `docs/decisions/`.

---

## 0. Document control

### 0.1 Purpose

This document is the contract for the first implementation of KRCompiler. It defines:

- what is in scope for v1.0, v1.1, and later releases;
- the target matrix and data models;
- the language feature matrix for C and C++;
- the compiler pipeline, IR, backend framework, and assembler;
- the ABI, object format, linker, symbol/debug format, and runtime;
- the corpus, conformance strategy, acceptance gates, and milestones;
- the risks and open questions.

It is a specification, not a task tracker. The live task tracker will be `implementation.md`; design details that are too large for this document will live in `docs/` and MUST be approved before the milestone that depends on them.

### 0.2 Approval and change control

- This plan was approved on 2026-11-09; the §18.1 checklist and §18.3 sign-off are complete. Implementation proceeds under §18.2.
- Every material change after approval MUST update this document and add a decision-log entry.
- Frozen contracts (ABI, object format, symbol format, target descriptor schema) MUST be versioned before code depends on them.
- The plan is expected to evolve. “Approved” means the direction, scope, and gates are accepted, not that every detail is final.

---

## 1. Executive summary

KRCompiler is one compiler for C and C++ that emits a typed, target-independent IR, then targets each virtual CPU through a per-target description and backend. It includes its own assembler, linker, object format, symbol/debug format, and common runtime. The design goal is not to clone GCC or Clang; it is to give the KR-family machines a coherent, debuggable, self-owned toolchain that can grow from C to practical modern C++.

### 1.1 Architecture at a glance

```text
.c / .cpp
   -> driver
   -> source manager + diagnostics
   -> preprocessor
   -> lexer
   -> C parser  /  C++ parser        (shared AST, dialect-aware Sema)
   -> semantic analysis              (types, scopes, overloads, templates,
                                      constexpr, cleanup, vtable/RTTI metadata)
   -> typed SSA IR                   (explicit memory, volatile, atomics, EH,
                                      debug locations, source types)
   -> optimiser (-O0 / -O1)
   -> target lowering                (instruction selection, regalloc, frames)
   -> assembly text  ->  assembler  ->  object writer
   -> krld (linker)  ->  target image writer  ->  VM
   -> symbol/debug sections  ->  shared reader  ->  DAP / GUI debugger
```

### 1.2 v1.0 scope (target: month 12; 6–9 months for a usable milestone at M5/M6)

- `krc` and `krld` build and run the **Tier 0–2** corpus on **KR32**, and the **Tier 0–1** corpus on **KR16**.
- KR32: C11 subset, C++11 core plus the selected C++14 and C++17 features, and a source-level debugger.
- KR16: the C subset only in v1.0.
- A minimal runtime sufficient for the corpus: startup, allocator, string/memory functions, integer/string `printf`, basic file I/O through a common host-service ABI, and static TLS.
- One object format, one linker, one binary symbol/debug format, and one shared debugger reader.

### 1.3 v1.1 scope (target: months 12–18)

- Exceptions and RTTI.
- Variadic templates.
- Dynamic `thread_local` initialization.
- `std` core utilities and memory (`unique_ptr`, `shared_ptr`, `move`, `forward`, `pair`, tuple basics).
- Full KR32 `libc.c` and `kr32os` build, plus KR16 `libc.s`/`resources/fileio.c` integration (Tier 3).
- Matrix and TLS examples become regular regression tests.

### 1.4 Non-goals

- Full ISO C++11 conformance in v1; two-phase lookup, SFINAE, and concepts are later.
- A complete C++ standard library; `string`/`vector`/`iostream` arrive in later releases.
- Modules, coroutines, ranges, C++23, or the full C++20 library in v1.
- A 64-bit backend, a stack-machine backend, or an 8-bit backend in v1.
- PIC/PIE, shared libraries, dynamic linking, LTO, JIT, or self-hosting.
- ABI compatibility with MSVC, GCC, Clang, libstdc++, or libc++.
- Inline assembly; target-specific operations are exposed as builtins and attributes.
- Debugging optimised code beyond line/function mapping in v1.
- C11 VLAs, `_Complex`, `_Atomic`/`<stdatomic.h>`, or `<threads.h>`.

### 1.5 Biggest risks

1. **No target sysroot exists.** Both existing front ends synthesise standard headers inside compiler code. A real, target-aware sysroot is a new v1 deliverable and a prerequisite for a retargetable compiler.
2. **The C++ front end is the largest subsystem.** The selected C++11/14/17 surface is practical but still substantial; templates, overload resolution, and cleanup semantics are the critical path.
3. **The corpus is not a conformance suite.** Feature coverage lives inside host doctest strings and must be extracted; the C++ target corpus is six example files, only three substantial.
4. **`libc.c` and `kr32os` are large, freestanding, and non-conforming in places.** They are both test material and runtime source; migrating them is real work.
5. **The KR16 16-bit profile is deliberately non-conforming.** It is acceptable only as a documented embedded profile, not as “C++11 on 16 bits.”

---

## 2. Goals, non-goals, and success criteria

### 2.1 Goals

| ID | Goal | Acceptance |
|---|---|---|
| G-1 | One compiler for C and C++ | `krc` accepts `.c` and `.cpp` with explicit dialect modes; shared front-end infrastructure; no “parse C as C++” shortcut. |
| G-2 | Retargetable IR | The same IR feeds KR32 and KR16 backends without target-specific front-end changes. |
| G-3 | Per-target backends | Target descriptions and hooks cover data model, ABI, encoding, layout, and capabilities. |
| G-4 | One linker | `krld` links objects and archives for all supported targets, with target-tagged validation. |
| G-5 | One symbol/debug format | All debuggers consume one shared reader; no per-VM symbol dialects. |
| G-6 | Common runtime | One runtime source tree plus a HAL compiles for every supported target. |
| G-7 | Practical modern C++ | C++11 core plus selected C++14/17 in v1.0; a credible path to selected C++20/23. |
| G-8 | Teaching quality | Readable IR/assembly dumps, clear diagnostics, deterministic output, debuggable `-O0`. |
| G-9 | Migration | KR32/KR16 toolchains coexist until the new compiler covers the corpus, then cut over. |

### 2.2 Non-goals for v1.0

- Full C++11 conformance; full standard library; concepts/SFINAE/two-phase lookup; modules/coroutines/ranges; C++23.
- 64-bit, stack-machine, and 8-bit backends.
- Dynamic linking, shared objects, PIC/PIE, LTO, JIT, self-hosting.
- Debugging fully optimised code; inline-frame reconstruction.
- Host-header compatibility or host ABI compatibility.

### 2.3 Success criteria

**v1.0 is successful when:**

1. `krc` compiles and `krld` links the Tier 0–2 corpus for KR32 and Tier 0–1 for KR16, with expected output and exit codes.
2. `hello.cpp`, `balanced-tree.cpp`, and the extracted C++ core fixtures run on KR32.
3. A debugger session on KR32 can set source breakpoints, show the call stack, inspect locals/params/globals/statics, expand structs/classes/arrays/pointers, evaluate simple expressions, and show threads.
4. `-O0` output is deterministic and byte-identical across repeated builds on the same host.
5. The build is clean under warnings-as-errors on Windows MSVC and Linux GCC/Clang, and the test suite passes.
6. `docs/abi.md`, `docs/object.md`, `docs/symbols.md`, `docs/ir.md`, and the target profiles are written and versioned.

**v1.1 is successful when:**

1. Exceptions and RTTI work across translation units.
2. Variadic templates and dynamic `thread_local` work.
3. `std` utilities/memory are usable.
4. KR32 `libc.c`/`kr32os` build and run, and the KR16 `libc.s`/`fileio.c` runtime is integrated (Tier 3).
5. Matrix and TLS examples are regular tests, not opt-in experiments.

---

## 3. Decision summary

The full decision log is in Appendix A. The architecture-defining decisions are:

| ID | Decision | Value |
|---|---|---|
| D-001 | Dialects | One driver, two dialects. Shared source manager, preprocessor, lexer, AST, IR, and backend; dialect-aware Sema; separate C and C++ parse paths where the grammars conflict. |
| D-002 | Targets | KR32 is the v1 reference; KR16 is the v1 C target; the 64-bit target is a placeholder; stack/8-bit targets are out of scope. |
| D-003 | Tool shape | `krc` compiler driver, `krld` linker, shared parser/reader libraries; monorepo; `--target` selects the backend. |
| D-004 | ISA policy | ISAs are frozen by default; small, versioned additions are allowed with a target feature flag, documentation, VM change, and golden encoding vectors. |
| D-005 | IR | Typed SSA in memory plus a textual dump. No stable serialised IR in v1. |
| D-006 | IR contents | Explicit exceptions/cleanup, volatile/MMIO, atomics, debug locations, vtable/RTTI metadata, and source-level types. No address spaces/banking or inline/call-graph metadata in v1. |
| D-007 | Backend | Hybrid: shared instruction selection, regalloc, frame layout, and peepholes, plus C++ constexpr target descriptions and small hand-written hooks. |
| D-008 | Output | Assembly text by default; objects through a shared writer; `-S` and `-c` share encoding tables. |
| D-009 | Assembler | One assembler framework with per-target syntax and encoding modules. |
| D-010 | ABI | One versioned C/C++ ABI, inherited from KR32 where proven, with redesigned `.cpp_except` records and pointer-width/endianness parameters. |
| D-011 | ABI stability | ABI is tied to the compiler version; the runtime is rebuilt each release. No cross-version binary compatibility. |
| D-012 | Data models | Strict on 32/64-bit; documented IP16 embedded profile on KR16; LP64 for the future 64-bit target; per-target plain `char` with `__CHAR_UNSIGNED__`. |
| D-013 | Object format | New unified target-tagged container with 64-bit canonical addresses and offsets. |
| D-014 | Linker | Static objects/archives, weak/COMDAT, section GC, relocation overflow checks, branch relaxation, map/size reports, linker layout scripts, debug merge/strip. No PIC/PIE, shared/dynamic, or LTO. |
| D-015 | Images | One linker core plus per-target image writers initially; converge later if useful. |
| D-016 | Symbols/debug | One binary, versioned, target-tagged format plus a canonical text/JSON dumper and a shared reader library. |
| D-017 | Debug features | Breakpoints, call stack, locals/params/globals/statics, aggregate expansion, inheritance/virtual/dynamic type, expressions/watches, threads/cores, memory/disassembly. Template display names and inline frames are later. |
| D-018 | Optimised debug | `-O0` fully debuggable; `-O1` line/function only in v1. |
| D-019 | Debug storage | In-image by default; optional sidecar linked by build ID. |
| D-020 | Runtime | Common source plus HAL plus per-target crt/syscall/soft-float/unwinder stubs. |
| D-021 | Library ladder | v1 freestanding; v1.1 core utilities/memory; v1.2 `string`/`vector`/`algorithm`; v1.3 `iostream` + `map`/`unordered_map`. |
| D-022 | Threading | Guest threads, atomics, and guards in the runtime; no `std::thread` surface in v1. |
| D-023 | C mode | C11 conveniences plus C99 designated/compound initializers; no VLAs, `_Complex`, `_Atomic`, `<threads.h>`, K&R definitions, or implicit declarations. |
| D-024 | C++ ladder | C++11 core + C++14 + selected C++17 core in v1.0; selected C++20 language essentials in v1.2; concepts after SFINAE/two-phase; no C++20 library or C++23. |
| D-025 | Template machinery | SFINAE and two-phase lookup are post-v1; concepts follow them. |
| D-026 | Inheritance | Single inheritance in v1.0; multiple and virtual inheritance in v1.2. |
| D-027 | Attributes | Basic standard attributes plus OS attributes (`interrupt`, `naked`, `noreturn`, custom calling conventions). |
| D-028 | Corpus | Tier 0–2 on KR32 and Tier 0–1 on KR16 for v1.0; Tier 3 for v1.1; Tier 4 is stretch. |
| D-029 | Sysroot | Real headers, generated target `bits/` headers, no host system headers. |
| D-030 | v1 boundary | v1.0 is a usable toolchain plus debugger; v1.1 adds EH/RTTI, variadic templates, dynamic TLS, utilities/memory, full KR32 `libc.c`/`kr32os` and KR16 runtime integration. |
| D-031 | Migration | Coexist, then cut over when the corpus is green; clean object-format break. |
| D-032 | Testing | Differential against host compilers, curated conformance, extracted fixtures, golden IR/assembly/symbol tests. |
| D-033 | Self-hosting | Not a goal; the compiler runs on the host only. |
| D-034 | Process | MIT; KR32/KR16 documentation and handoff conventions; ADRs for frozen contracts. |
| D-035 | Determinism | Byte-identical outputs, sorted symbols, path remapping, build IDs and source hashes. |

---

## 4. Targets and data models

### 4.1 Target matrix

| Property | KR32 | KR16 | KR64 (placeholder) |
|---|---|---|---|
| Status | v1 reference | v1 C-only | later |
| Word/pointer width | 32 | 16 | 64 |
| Endianness | little | little | little |
| Data model | ILP32 | IP16 embedded profile | LP64 |
| `char` | 8, signed | 8, unsigned | TBD |
| `short` / `int` / `long` / `long long` | 16 / 32 / 32 / 64 | 16 / 16 / 16 / none | 16 / 32 / 64 / 64 |
| Pointer | 32 | 16 | 64 |
| `float` / `double` / `long double` | 32 / 64 / 64 (alias) | none | 32 / 64 / 64 (alias) |
| `size_t` / `ptrdiff_t` | 32 | 16 | 64 |
| `intmax_t` | 64 | 16 (profile) | 64 |
| Registers | R0–R15; F0–F15; T0–T15; R7 FP; R11 TP | R0–R4; SP/SR/PC; R4 FP | TBD |
| FPU / matrix | yes / yes | no / no | TBD |
| Atomics / guest threads | LL/SC; cluster threads | no | TBD |
| EH / RTTI | v1.1 / v1.1 | no | TBD |
| TLS | static; TP = R11; dynamic v1.1 | no | TBD |
| OS services | `kr32os` host services | BIOS `SYS` | TBD |
| Image | KR32 native/new | KR16 native/new | TBD |

### 4.2 Target descriptor

A target descriptor is a C++ `constexpr` table (plus small hooks) with these sections:

| Section | Fields |
|---|---|
| Identity | `id`, `name`, `version`, `family` (register/accumulator/stack), `pointer_width`, `endianness`, `max_alignment`, `address_space_count` (1 in v1). |
| Data model | Sizes, alignments, signedness for `char`/`short`/`int`/`long`/`long long`/pointer/`float`/`double`/`long double`/`wchar_t`/`char16_t`/`char32_t`; `intmax_t`; `FLT_*`/`DBL_*` limits; `__CHAR_UNSIGNED__`. |
| Registers | Register classes, counts, fixed roles (SP, FP, TP, PC, flags), caller/callee saved, reserved registers. |
| Instructions | Patterns for IR operations, addressing modes, immediate ranges, branch ranges, encoding size, cost model, relaxation rules. |
| ABI | Argument/return registers, stack alignment, sret threshold, varargs rules, TLS model, EH/RTTI conventions, aggregate passing. |
| Object/image | Section names and flags, relocation types, relaxation, entry symbol, native image header, load address rules. |
| Capabilities | FPU, matrix/transform, atomics, threads, EH, RTTI, TLS, host services, file I/O, time, `volatile` rules. |
| Feature macros | Target macros (`__kr32__`, `__kr16__`, `__kr64__`), capability macros, standard revision macros. |
| Constraints | Max object size, max alignment, supported section types, unsupported features and their diagnostics. |

The descriptor is compiled into `krc`; it is not a runtime plugin in v1. A data-file descriptor format MAY be added later, but C++ tables are the v1 contract (D-007).

### 4.3 ISA change policy

- ISAs are frozen by default. The compiler MUST adapt to the documented ISA.
- A small ISA addition is allowed only when a compiler/runtime requirement cannot be met reasonably otherwise. Examples: a stack-pointer restore for the unwinder, an unaligned load, an atomic primitive.
- Every addition MUST have: a target descriptor feature flag, an ISA document update, a VM implementation, a golden encoding vector, an assembler/disassembler update, and a migration note.
- No addition may silently change an existing encoding or flag semantic.

### 4.4 64-bit target

The 64-bit target is not defined yet. The plan requires pointer-width independence and reserves:

- LP64 as the default data model, subject to the 64-bit VM specification.
- 64-bit canonical fields in the object, symbol, and debug formats.
- A target descriptor slot `kr64` (name placeholder).
- ABI and backend interfaces that do not assume 32-bit pointers, `long`, or addresses.

The 64-bit VM specification is a separate deliverable; the compiler MUST NOT ship a 64-bit backend before that specification and its ISA are approved.

### 4.5 Out-of-scope target families

Stack machines (SR16/NemoCPU) and 8-bit accumulator machines (A6502) are out of scope for v1. The IR has no address-space/banking model yet, and the runtime assumes a byte-addressable flat memory. If these targets become a requirement, the IR gains an address-space extension and a separate backend family; that is a new project phase, not a KR32/KR16 variant.

---

## 5. Corpus, conformance, and acceptance

### 5.1 Inventory summary

The 2026-11-09 inventory found:

- **Target-facing code is small:** approximately **24.6k LOC** in KR32 (examples, `libc.c`, `kr32os`, tests) and **9.4k LOC** in KR16 (examples, C tests, `fileio.c`, experimental C++ tests). These aggregates sum those target-facing directories and exclude the host toolchain, third-party trees, and font data.
- **The single most valuable C file** is `../kr32/src/cc/libc.c` (3,058 non-blank lines; 3,239 total): allocator, buffered stdio, `printf`/`scanf`, `str*`/`mem*`, `strto*`, `qsort`, POSIX-ish file I/O, and the C++ allocation hooks.
- **The best C stress test** is `../kr16/tests/kevin/C-for-U.c` (1,473 LOC), a struct-free C compiler with deep recursion and roughly 60 functions.
- **The C++ target corpus is six example files, only three substantial:** `../kr32/examples/hello.cpp`, `balanced-tree.cpp`, and `matrix.cpp` carry most of the C++ surface; `cxx_tls.cpp`, `cxx_libc.cpp`, and `FileOpen.cpp` are small. The experimental KR16 C++ tests total 97 lines.
- **There is no standalone target conformance corpus.** C11/C++ feature coverage lives as escaped string literals inside host doctest files (`../kr32/tests/unit`, `../kr32/tests/cxx`). The `test_cc_*` and `test_cxx_codegen_*` files total 8,216 lines; the embedded C++ source subset is smaller.
- **There are no target headers.** Both front ends synthesise `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<string>`, `<iostream>`, `<thread>`, `<atomic>`, and others inside compiler source (`../kr32/src/cc/cc.cpp:590`, `../kr32/src/cxx/preprocessor.cpp:3125+`). A real sysroot is a new v1 deliverable (D-029).
- **No inline assembly** exists in the corpus; privileged code is in `.S` files. C only declares externs.
- **KR32 is ILP32 with 32-bit `long` and signed `char`; KR16 uses a 16-bit `long` and unsigned `char`.** The “16-bit long” assumption is KR16-only.
- **No variadic templates and no multiple/virtual inheritance** appear in the corpus. They are selected in the language ladder but have no corpus dependency.
- **The corpus contains non-conforming C and non-standard library assumptions:** `void main`, implicit declarations, `%h` used for pointers, `_heapwalk`, POSIX file calls in ISO-C-looking files, and MMIO pointers without `volatile`.

### 5.2 Corpus tiers

| Tier | Content | Target | v1.0/v1.1 |
|---|---|---|---|
| **0 — Smoke** | hello/arithmetic/functions | KR32, KR16 | v1.0 |
| **1 — C core** | structs, pointers, arrays, control flow, preprocessor, varargs, `printf` | KR32, KR16 | v1.0 |
| **2 — C++ core** | classes, single inheritance, virtual, `new`/`delete`, templates, lambdas, `constexpr`; RTTI/EH fixtures in v1.1 | KR32 | v1.0 (EH/RTTI fixtures v1.1) |
| **3 — Runtime/OS** | KR32: `libc.c`, `kr32os`; KR16: `libc.s`, `resources/fileio.c` | KR32, KR16 | v1.1 (partial in v1.0) |
| **4 — Stress** | `C-for-U.c`, extracted C++ doctest fixtures, cc11/17/23 compiler sources | KR32, KR16 | stretch |

The compile-first list is in Appendix B.

### 5.3 Sysroot

- `krc` MUST use a real sysroot tree, not synthesised headers.
- Layout:
  - `sysroot/include/` — common C and C++ headers.
  - `sysroot/<target>/include/` — target-specific headers and generated `bits/` (`stdint.h`, `limits.h`, `float.h`, `stddef.h`, `stdarg.h`, `stdbool.h`, `stdalign.h`, `stdnoreturn.h`).
  - `sysroot/<target>/lib/` — prebuilt runtime objects/archives.
- The compiler searches its sysroot by default, then `-I`/`-isystem` paths. Host system headers MUST NOT be searched implicitly.
- `-nostdinc` and `--sysroot` are supported.
- Generated `bits/` headers are produced from the target descriptor, so `sizeof(long)` and `INT_MAX` cannot drift from the ABI.

### 5.4 Feature extraction from doctest strings

Because the conformance material is embedded in host doctest files, M0/M1 MUST deliver `krc_extract_fixtures` (`src/tools/extract_fixtures`):

- Scan `../kr32/tests/unit` and `../kr32/tests/cxx` for escaped C/C++ source strings.
- Extract each into `tests/corpus/extracted/<feature>/<name>.c` or `.cpp`.
- Record the original file:line and the feature tag.
- Provide expected output or a run/exit expectation where the original test has one.
- Compile and run extracted fixtures on the host compiler and on the target VM, then compare.

This is a test-infrastructure requirement, not a nice-to-have: without it, the compiler has no regression coverage for templates, exceptions, RTTI, lambdas, or `constexpr`.

### 5.5 Corpus migration checklist

The following changes are required in the corpus or its build flags before the corresponding gate:

1. **MMIO `volatile`:** add `volatile` to MMIO pointers in `kr32os/kernel/console.c`, `timer.c`, `syscall.c`, and similar locations. The compiler MUST honour `volatile`; it cannot fix a missing qualifier.
2. **`printf` standardisation:** replace `%h` pointer printing with `%p`; implement standard length modifiers (`h`, `hh`, `l`, `ll`, `j`, `z`, `t`) in the new `printf`. Add a compatibility warning for non-standard uses.
3. **C conformance:** change `void main` to `int main`; add prototypes/includes; remove implicit declarations; replace `void *malloc(int)` with the standard declaration. Strict v1 diagnostics will reject these otherwise.
4. **`long` assumptions:** audit KR16 C for code that assumes 32-bit `long`; use `int32_t`/lo-hi structs or the documented IP16 profile.
5. **Non-standard APIs:** guard `_heapwalk`, `sys_open_dialog`, `sys_random`, and POSIX-only file calls behind target profiles or replace them with the common host-service ABI.
6. **Headers:** replace hand-written prototypes and synthesised headers with real sysroot includes.
7. **Feature flags:** gate `matrix.cpp`, `cxx_tls.cpp`, and thread/atomic examples behind target-capability flags so a partial build remains green.
8. **Fixtures:** extract the doctest source strings into the standalone corpus.

### 5.6 Conformance strategy

| Layer | What it proves | Gate |
|---|---|---|
| **Differential** | Same portable program produces the same stdout/exit code on host and VM | Every Tier 0–2 program where a host shim is possible |
| **Extracted fixtures** | C++ templates, exceptions, RTTI, lambdas, `constexpr` compile and run on the target | C++ gate |
| **Curated conformance** | C11/C++11/14/17 feature semantics not covered by the corpus | Per language milestone |
| **Golden IR/assembly/symbol** | Lowering, encoding, and debug records are stable | Every merge |
| **Negative diagnostics** | Unsupported features and target-profile violations produce the specified errors | Every feature |
| **Fuzzing** | Lexer/preprocessor/parser robustness | Post-v1.0 |

Differential testing uses a host shim:

- The target program writes through the common host-service ABI (`putchar`, `write`, `exit`).
- The host harness provides a shim implementation of that ABI, so the same source can be compiled by the host compiler with `-DKR_HOST_TEST`.
- For target-specific code (MMIO, page tables, privileged instructions), differential testing is not applicable; golden execution or VM-level tests are used instead.

### 5.7 Acceptance gates

| Gate | Requirement |
|---|---|
| **G0** | Repository builds, CI is green, docs skeleton and fixture extractor exist. |
| **G1** | `krc -fsyntax-only` accepts Tier 1 C on **KR32** and rejects unsupported features with the specified diagnostics. KR16 syntax is gated at G7. |
| **G2a** | `hello.c` and `mainentry.c` compile to KR32 assembly with correct call/return sequences; no object, link, or run yet. |
| **G2b** | `hello.c` and `mainentry.c` assemble, link, and run on KR32 with correct exit codes. |
| **G3** | Tier 1 C runs on KR32 with expected output, including `binarytree.c`, `libc-test.c`, and `varargs.c`. |
| **G4** | A KR32 debugger session on `binarytree.c` supports breakpoints, stack, locals/globals, aggregate expansion, and memory view. |
| **G5** | `hello.cpp` and `balanced-tree.cpp` run on KR32; classes, single inheritance, virtual dispatch, and `new`/`delete` work. |
| **G6** | Extracted C++11/14/17 core fixtures pass: templates, lambdas, move, `auto`/`decltype`, range-for, `constexpr`, `initializer_list`, `noexcept`/`alignas`, `if constexpr`, structured bindings, inline variables. Fold expressions follow with variadic templates in v1.1. |
| **G7** | Tier 0–1 C is accepted, assembled, linked, and run on KR16 with the IP16 profile and the KR16 ABI, including `hello.c` and `sum.c`. |
| **G8** | v1.0 corpus is green, outputs deterministic, and the debugger works for C and C++. |
| **G9** | v1.1: EH/RTTI, variadic templates, dynamic TLS, utilities/memory, full KR32 `libc.c`/`kr32os`, and KR16 `libc.s`/`resources/fileio.c` integration are green. |

A gate is claimed only by the milestone that can produce its evidence. M2 emits KR32 assembly (G2a), M3 links and runs it (G2b), and M7 owns KR16 syntax, link, and run (G7).

---

## 6. Language scope

### 6.1 C mode (`-std=c11`)

**Supported in v1.0**

- C89/C99 base: declarations, `//` comments, mixed declarations, `for`-scope declarations, `long long` where the target profile supports it, `_Bool`, `inline` (C99 semantics), `sizeof`, `offsetof`, bit-fields, unions, enums, typedefs, function pointers.
- C99/C11 initializers: designated initializers, compound literals, aggregate/string initializers.
- C11 conveniences: `_Generic`, `_Static_assert`, `_Alignas`/`_Alignof`, `_Noreturn`, anonymous structs/unions.
- Standard preprocessor: include guards, `#line`, and implementation-defined `#pragma` pass-through where harmless.
- Extensions accepted in C mode, not claimed as C11: `restrict` (accepted and treated as a no-op, so C11 restrict semantics are not provided), `#pragma once`, `__has_include`, `__COUNTER__`.
- `volatile` with exact-width, non-folded access semantics.
- Target builtins and attributes (see §6.4).

**Diagnosed as unsupported in v1.0**

- VLAs and variably modified types.
- `_Complex`/`<complex.h>`.
- `_Atomic`, `<stdatomic.h>`, `_Thread_local`, `<threads.h>`.
- K&R function definitions.
- Implicit function declarations and implicit `int`.
- `void main`.
- `typeof` and `__typeof__` (GNU extension; later).
- `__attribute__((packed))` (later; see §6.4).
- Flexible array members (later).
- Nested functions and GNU statement expressions.

**C library surface**

The initial sysroot covers the corpus: `assert.h`, `ctype.h`, `errno.h`, `float.h`, `limits.h`, `math.h`, `stdarg.h`, `stdbool.h`, `stddef.h`, `stdint.h`, `stdio.h`, `stdlib.h`, `string.h`, `time.h`, and the POSIX-ish file functions used by `libc.c` (`open`, `close`, `read`, `write`, `stat`, `getcwd`, `chdir`). `setjmp`/`longjmp` are later. The exact function list is in §14.3.

### 6.2 C++ mode (`-std=c++11`, `c++14`, `c++17`, `c++20`)

The compiler defaults to `c++11` for `.cpp` files until `c++17` is stable. `-std=c++17` enables C++14 plus the selected C++17 core.

**Tier v1.0 — C++11 core**

| Feature | Notes |
|---|---|
| Classes, access control, constructors/destructors, copy/move constructors, assignment | Single inheritance only. |
| Virtual functions, pure virtual, abstract classes, virtual destructors | Itanium-like vtable layout. |
| `new`/`delete`, `new[]`/`delete[]`, placement new | `operator new`/`delete` replacement overloads later. |
| Namespaces, nested names, `using` declarations | |
| References, `const` member functions, default arguments, `explicit` | |
| Function and operator overloading | Overload resolution for the supported subset. |
| Function templates, class templates, explicit/partial specialization | Implicit instantiation; weak/COMDAT dedup. |
| Lambdas with captures | By value/reference; `mutable`. |
| Move semantics and rvalue references | Move constructors/assignment, `std::move`/`forward` intrinsics. |
| `auto`, `decltype`, range-for, `nullptr`, `enum class` | |
| C++11 `constexpr` subset | Constant evaluation in Sema. |
| `std::initializer_list`, brace initialization | Compiler support plus a freestanding header. |
| `noexcept`, `alignas`/`alignof`, `static_assert` | `thread_local` static TLS only in v1.0. |
| `override`, `final`, `=default`, `=delete`, delegating constructors | |
| Basic attributes | `[[nodiscard]]`, `[[maybe_unused]]`, `[[deprecated]]`, `[[noreturn]]`. |

**Tier v1.0 — C++14 and selected C++17**

- C++14: generic lambdas, init captures, return type deduction, relaxed `constexpr`, variable templates.
- C++17 core: `if constexpr`, structured bindings, inline variables. Fold expressions land with variadic templates in v1.1.
- C++17 template/CTAD cluster, C++17 library (`optional`, `variant`, `any`, `string_view`, `byte`, `apply`/`invoke`), and filesystem/parallel algorithms are later.
- C++20 library is out of scope; C++23 is out of scope.

**Staged later**

| Feature | Tier | Reason |
|---|---|---|
| Exceptions (throw/try/catch, unwinding) | v1.1 | Needs EH ABI, tables, unwinder, cleanup codegen. |
| RTTI (`typeid`, `dynamic_cast`) | v1.1 | Needs typeinfo/vtable ABI and runtime. |
| Variadic templates | v1.1 | Selected for the ladder; no corpus dependency. |
| Dynamic `thread_local` initialization/destruction | v1.1 | Static TLS first; dynamic TLS needs guards and registration. |
| Multiple and virtual inheritance | v1.2 | Expensive layout/dynamic-cast work; no corpus use. |
| `std::string`, `std::vector`, `std::algorithm` | v1.2 | Requires class templates, allocators, EH, move. |
| C++20 `<=>`, designated init, `constexpr` enhancements | v1.2 | Language essentials, staged after C++17 is solid. |
| Concepts / `requires` | after SFINAE/two-phase | Constraint satisfaction needs the template machinery first. |
| SFINAE, `enable_if`, two-phase lookup, ADL | post-v1 | The largest front-end investment. |
| `std::iostream`, `std::stringstream` | v1.3 | Heavy; `printf` first. |
| `std::map`, `unordered_map`, `deque`, `list` | v1.3 | More containers and allocator work. |
| User-defined literals | later | Low priority. |
| `__attribute__((packed))`, `typeof` | later | Extensions; diagnose in v1. |
| Modules, coroutines, ranges, C++23 | later | Out of scope. |

**C/C++ interop**

- `extern "C"` uses the C ABI and unmangled names.
- C headers are usable from C++ through normal include paths.
- C++ mangling is confined to C++ symbols.
- `__cplusplus` and `__STDC_VERSION__` reflect the selected dialect.
- A `.c` file is never compiled as C++ implicitly; a `.cpp` file is never compiled as C.

### 6.3 Feature matrix per target

| Feature | KR32 | KR16 |
|---|---|---|
| C11 subset | yes | yes, minus `float`/`double`/`long long` |
| C++ core | yes | no (v1 C-only) |
| Matrix/transform builtins | yes | no |
| Atomics / guest threads | yes | no |
| EH / RTTI | v1.1 | no |
| TLS | static v1.0; dynamic v1.1 | no |
| Debugger | yes | C debugger |
| Host services | kr32os ABI | BIOS `SYS` |

An unsupported feature/target combination MUST produce a specific diagnostic, for example:

```text
error: C++ exceptions are not supported for target 'kr16' in this release
note: exceptions are supported for KR32 starting in v1.1
```

### 6.4 Target extensions and attributes

- Matrix/transform builtins on KR32: `__builtin_kr32_matrix_mul`, `_mul_add`, `_transform`, `_transpose`, `_dot`, `_push`, `_pop`, `_drop`, with `kr32/matrix.hpp` wrappers.
- Atomics: `__builtin_ll`, `__builtin_sc`, `__builtin_fence`, and `__atomic_*` intrinsics for the runtime.
- Host services: `__builtin_kr32_syscall` and the common host-service ABI.
- Volatile/MMIO: `volatile` accesses are exact-width and never folded; optional `__builtin_mmio_*` helpers.
- Attributes: `interrupt`, `naked`, `noreturn`, `section`, `aligned`, and custom calling-convention attributes. Syntax: `[[krc::interrupt]]` and `__attribute__((interrupt))`; the grammar and ABI effects are defined in `docs/attributes.md` before v1.0.
- No inline assembly in v1.0. If it is added later, it is a versioned language extension with a constraint model per target.

---

## 7. Compiler architecture

### 7.1 Pipeline

```text
 driver
   -> source manager (file/line/column, include stack, macro expansion)
   -> diagnostics (severity, notes, fix-its, error limit)
   -> preprocessor (tokens, macros, conditionals, includes, pragmas)
   -> lexer
   -> C parser or C++ parser (shared AST)
   -> Sema (symbols/scopes, types, conversions, overloads, access,
             templates, constexpr, cleanup, vtable/RTTI metadata)
   -> IRGen
   -> typed SSA IR
   -> verifier
   -> optimiser (-O0/-O1)
   -> target lowering (legalisation, instruction selection)
   -> register allocation
   -> frame layout
   -> peepholes
   -> assembly printer
   -> assembler (shared framework, target syntax/encoding)
   -> object writer
   -> krld
   -> target image writer
```

### 7.2 Front end

- **Source manager:** UTF-8 input, byte offsets plus line/column maps, include stack, macro expansion notes, `#line` remapping.
- **Diagnostics:** stable diagnostic IDs, source snippets, severity, notes, optional fix-its, deterministic ordering, error limit. Warnings are errors in CI.
- **Preprocessor:** C99/C++11 preprocessing, object/function-like macros, variadic macros, `#`/`##`, `_Pragma`, `#pragma once`, include guards, `__has_include`, `__COUNTER__`, `__FILE__`/`__LINE__`, `#line`, predefined target macros. No host headers.
- **Lexer:** shared token model with dialect-specific keywords and literals; raw strings later.
- **Parsers:** separate C and C++ parse paths over the shared AST. The C parser is a C11 recursive-descent parser. The C++ parser covers the selected C++11/14/17/20 subset; unsupported grammar produces a targeted diagnostic rather than a generic parse error.
- **Sema:** scopes and symbols; type system; usual arithmetic conversions; overload sets and ranking; access control; constructor/destructor semantics; copy/move; template argument deduction and instantiation; `constexpr` evaluation; cleanup/destructor insertion; vtable/RTTI metadata; `extern "C"`; attributes.
- **Templates:** instantiate in Sema and emit concrete functions/classes into IR. Weak/COMDAT linkage for inline/template entities. Two-phase lookup and SFINAE are post-v1; the compiler MUST document the simplified lookup model and reject code that depends on unsupported dependent-name rules.
- **Constexpr:** a tree-walking evaluator for the supported C++11 subset plus C++14 relaxed rules. `if constexpr`, `static_assert`, array bounds, and `initializer_list` use the same evaluator. `consteval`/`constinit` later.

### 7.3 IR generation

IRGen lowers the typed AST to SSA:

- scalar and aggregate loads/stores; address-of, pointer arithmetic, array indexing;
- calls, varargs, `sret`, by-value aggregates;
- constructors/destructors and cleanup;
- virtual dispatch, vtable/typeinfo emission;
- exception edges in v1.1;
- `volatile` and atomic operations;
- debug locations and variable locations;
- target builtins.

### 7.4 Library structure

- `src/frontend/` — source, diagnostics, preprocessor, lexer, C parser, C++ parser, AST, Sema, templates, constexpr.
- `src/ir/` — IR model, builder, verifier, printer, passes.
- `src/backend/` — target-independent lowering, instruction selection framework, regalloc, frame layout, peepholes; `targets/kr32`, `targets/kr16`.
- `src/asm/` — assembler framework, syntax modules, encoders, relocations.
- `src/linker/` — object reader/writer, linker core, layout scripts, image writers.
- `src/debug/` — symbol/debug format reader/writer, dumper, shared debugger reader.
- `krsrc/` — common runtime, HAL, per-target stubs, sysroot.
- `src/tools/` — `krc`, `krld`, `krsym` (optional dumper front end).
- `include/krc/` — public library APIs.
- `tests/` — unit, integration, differential, conformance, golden, corpus.
- `docs/` — specifications and ADRs.

---

## 8. IR specification

### 8.1 Goals

- Target-independent enough for KR32 and KR16.
- Typed enough for C++ semantics and debug information.
- Explicit about memory, `volatile`, atomics, exceptions, and debug locations.
- Simple enough to inspect and teach.
- Not a stable serialised artifact in v1.

### 8.2 Module and function model

```text
module
  target_profile
  type_table
  globals
  functions
  debug_metadata

function
  name, linkage, calling_convention, attributes
  parameter list (type, name, flags)
  basic blocks (entry first)
  exception table (v1.1)
  frame info
  debug info

basic_block
  instructions
  phi nodes at entry
  terminator (branch, condbranch, switch, invoke, return, unreachable)

instruction
  result type and virtual register (SSA)
  operands
  memory effects
  alignment
  debug location
```

### 8.3 Types

- `void`, `bool`, `i8`/`i16`/`i32`/`i64`, `f32`/`f64`.
- `ptr` (address space 0 only in v1), `array`, `struct`, `union`, `enum`, `function`, `member_ptr`, `vector` (reserved, unsupported in v1).
- `opaque` for incomplete types and forward declarations.
- References are lowered to pointers in IR; source-level reference types remain in debug metadata.
- `wchar_t`, `char16_t`, `char32_t` are target-dependent integer types in IR.
- Source-level types are retained in a side table for debug info and ABI lowering.

### 8.4 Memory and effects

- Explicit `load`/`store` with type, alignment, `volatile`, and atomic ordering.
- `alloca` for VLAs only if VLAs are added later; v1 uses fixed frame slots.
- `memcpy`/`memset` intrinsics for aggregate copies and initialization.
- No strict-aliasing assumptions in v1; the optimiser MUST NOT introduce type-based aliasing optimisations.
- `volatile` loads/stores are never removed, reordered across other volatile operations, or combined.
- Atomics: `load`, `store`, `cmpxchg`, `atomicrmw`, `fence` with `seq_cst` first; weaker orders only when the target supports them and the runtime tests them. Single-core targets may lower atomics to interrupt-disable/lock-free primitives.

### 8.5 Control flow and calls

- Direct and indirect calls; varargs; `sret`; by-value aggregates.
- `invoke` and `landingpad` for exceptions (v1.1); cleanup edges for destructors.
- Tail calls are not required in v1; the IR may mark them for later.
- Switch lowered to a jump table or compare chain by the backend.

### 8.6 Vtables and RTTI

- Vtable/typeinfo objects are globals with explicit layout and relocations.
- The vtable pointer is stored at offset 0 of a polymorphic object; the RTTI pointer is at vtable slot -1 (Itanium-like).
- Virtual calls load the function pointer from the vtable at a fixed slot.
- `dynamic_cast`/`typeid` lower to runtime calls/typeinfo comparisons (v1.1).

### 8.7 Debug information in IR

- Every instruction may carry a source location.
- Variables carry a storage location: frame offset, register, address, TLS offset, or a location expression.
- `-O0` uses frame offsets; `-O1` may use registers and location expressions, but v1 only guarantees line/function mapping at `-O1`.
- Inline/call-graph metadata is reserved but not emitted in v1.

### 8.8 Optimisation levels

**`-O0`:** no semantic-changing optimisation; straightforward lowering; all variables in frame slots; full debug info.

**`-O1`:** mem2reg, constant folding, dead code elimination, common subexpression elimination, copy propagation, CFG simplification, branch folding, peepholes, graph-colouring register allocation. No inlining, LTO, loop-invariant code motion, or vectorisation in v1.

The optimiser MUST preserve:

- `volatile` semantics;
- atomic ordering;
- exception/cleanup edges;
- debug line/function mapping (best effort at `-O1`);
- deterministic output.

### 8.9 Textual dump

`krc --dump-ir` prints a stable-for-humans (not stable-for-parsing) form:

```text
func @main() -> i32 {
entry:
  %p = alloca i8, 16, align 4
  %x = load i32, ptr %p, align 4
  %y = add i32 %x, 1
  store i32 %y, ptr %p, align 4
  ret i32 0
}
```

Dumps are for debugging and teaching; they are not a compatibility surface.

### 8.10 Verifier

The verifier checks:

- SSA dominance and single assignment;
- type consistency of operands and results;
- terminator presence and block references;
- phi-node predecessors;
- exception edge/landingpad pairing;
- alignment and volatility flags;
- target-profile legality.

A failed verification is a compiler bug and MUST abort with a diagnostic, not emit code.

---

## 9. Backend and target descriptions

### 9.1 Instruction selection

- Lower IR to a target DAG or a linearised machine IR.
- Pattern-match target instructions; legalise unsupported operations into libcalls or instruction sequences.
- Target hooks handle unusual operations (matrix tiles, transform stack, atomics, syscalls).
- Branches and displacements carry relaxation metadata for the assembler/linker.

### 9.2 Register allocation

- Graph-colouring allocator shared across register targets, with target constraints (fixed argument/return registers, reserved registers, callee-saved sets, register classes).
- Spilling to frame slots; rematerialisation of constants and addresses.
- KR16 has a very small register file (R0–R4 plus SP/SR/PC); the allocator MUST favour short live ranges and cheap spilling.
- KR32 has 16 GPRs plus FPRs and tile registers; tile allocation is limited to expression temporaries and caller-saved use.
- Stack/accumulator targets will need a separate allocation strategy; not in v1.

### 9.3 Frame layout

- Incoming arguments, saved registers, locals, spills, outgoing arguments, varargs save area, alignment padding.
- Stack alignment: 16 bytes at public call boundaries on KR32; 2 bytes on KR16 (to be confirmed in the KR16 ABI appendix).
- `sret` pointers and by-value aggregate copies follow the ABI.
- Debug frame records describe saved FP/SP and return-address locations.

### 9.4 Assembly and object emission

- The backend emits target assembly text by default.
- The same encoding tables drive a direct object writer (`-c`), avoiding a text round-trip.
- The assembler framework parses each target’s existing syntax (or a documented subset), resolves labels, applies relocations, relaxes branches, and writes the common object format.
- Assembly output is a first-class teaching/debugging artifact; it MUST be readable and include source/line comments where practical.

### 9.5 KR32 backend notes

- 32-bit little-endian; 4/8-byte instructions; inline 11-bit immediates with extension words.
- R0–R15; R7 frame pointer; R11 thread pointer; F0–F15; T0–T15 tiles.
- 16/32-bit loads/stores; indexed addressing; PC-relative and absolute modes.
- Branch relaxation for long displacements.
- FPU and matrix operations are optional capabilities but present on the reference target.
- Syscalls and host services use the kr32os ABI.

### 9.6 KR16 backend notes

- 16-bit little-endian; 16/32-bit instruction forms; 64 KiB address space.
- R0–R4 plus SP/SR/PC; R4 frame pointer; R0 return; R1–R3 arguments.
- No FPU; no 64-bit integer type in the IP16 profile; 32-bit values are library/lo-hi constructs.
- Branch relaxation is mandatory (ADR 0003).
- The code-size budget is tight; section GC and size-oriented peepholes matter.
- v1.0 compiles C only for KR16; C++ support is a later decision.

### 9.7 Future backend families

Stack machines and 8-bit accumulators need a different lowering model (stack-depth scheduling, accumulator allocation, tiny frames). The IR and target-descriptor interfaces SHOULD leave room for them, but v1 MUST NOT contort the register backend to accommodate them.

---

## 10. ABI specification

### 10.1 Data models

**KR32 (ILP32)**

| Type | Size | Align | Notes |
|---|---:|---:|---|
| `char` | 1 | 1 | plain `char` is signed; `signed char`/`unsigned char` explicit |
| `short` | 2 | 2 | |
| `int` | 4 | 4 | |
| `long` | 4 | 4 | |
| `long long` | 8 | 8 | 64-bit division is a runtime libcall |
| pointer | 4 | 4 | function pointers included |
| `float` | 4 | 4 | IEEE binary32 |
| `double` | 8 | 8 | IEEE binary64 |
| `long double` | 8 | 8 | alias for `double` |
| `size_t`/`ptrdiff_t` | 4 | 4 | |
| `intmax_t` | 8 | 8 | |
| `wchar_t` | 4 | 4 | |
| `_Bool` | 1 | 1 | |

**KR16 (IP16 embedded profile)**

| Type | Size | Align | Notes |
|---|---:|---:|---|
| `char` | 1 | 1 | unsigned; `__CHAR_UNSIGNED__` |
| `short` | 2 | 2 | |
| `int` | 2 | 2 | |
| `long` | 2 | 2 | **not ISO-conforming**; documented profile |
| `long long` | — | — | unsupported; diagnosed |
| pointer | 2 | 2 | |
| `float`/`double` | — | — | unsupported; diagnosed |
| `size_t`/`ptrdiff_t` | 2 | 2 | |
| `intmax_t` | 2 | 2 | profile-limited |
| `wchar_t` | 2 | 2 | |

The KR16 profile is non-conforming by design. It exists to keep the 16-bit target practical; it is not a claim of C11 or C++11 conformance. Every diagnostic that depends on the profile MUST say so.

**KR64 (placeholder, LP64)**

`int` 32, `long` 64, `long long` 64, pointer 64, `short` 16, `char` 8, `float` 32, `double` 64, `long double` alias 64. To be confirmed with the 64-bit VM specification.

**Bit-fields and alignment.** Bit-field allocation follows the target ABI appendix: the allocation unit is the declared type’s storage unit, bit order is little-endian, and a bit-field does not straddle its allocation unit unless the target appendix explicitly allows it. `alignas` may increase alignment up to the target’s maximum; over-aligned dynamic allocation (`alignas(64)` on `new`) is not supported in v1.0. Packed structs are a later extension.

### 10.2 Calling conventions

**KR32** inherits `../kr32/docs/abi.md` revision 1:

- Integer arguments in R0–R3; floating-point arguments in F0–F7; further arguments on the stack.
- Integer return in R0 (R0/R1 for 64-bit), FP return in F0/F1.
- R7 is the frame pointer; R11 is the thread pointer.
- Public call boundaries are 16-byte aligned.
- Aggregates larger than 8 bytes use a hidden `sret` pointer.
- Variadic arguments are stack-passed in 8-byte slots; `float` promotes to `double`.
- `this` and member-function conventions follow the documented KR32 C++ ABI.
- Tile registers are caller-saved; matrix values cross calls by pointer/reference.
- The transform stack is per-thread and owned by the caller.

**KR16** (new appendix, to be frozen before M7):

- Integer arguments in R1–R3; further arguments on the stack; return in R0.
- R4 is the frame pointer; SP is full-descending.
- Stack alignment is 2 bytes; aggregates are passed by value on the stack.
- Variadic arguments are stack-passed; `va_list` points at the first stack slot.
- No FP/aggregate-register conventions.
- `this` is passed as the first integer argument.
- The exact rules MUST be documented in `docs/abi-kr16.md` and tested with a calling-convention suite.

**KR64** (placeholder): LP64, first six integer args in R0–R5, first eight FP args in F0–F7; to be confirmed.

### 10.3 Name mangling

- Itanium-like grammar with the `_Z` prefix.
- Builtin type codes, nested names, substitutions, operator names, template arguments, and function types follow the documented grammar in `docs/abi.md`.
- `extern "C"` symbols are unmangled.
- Vtable/typeinfo names use `_ZTV`, `_ZTI`, `_ZTS` with the mangled type.
- The compiler ships a demangler and uses source-level names in debug info.

### 10.4 Vtables, RTTI, and exceptions

- Itanium-like vtable layout: vtable pointer at offset 0; RTTI pointer at slot -1; virtual function slots in declaration order; virtual destructor slots for complete/deleting variants.
- `type_info` layout: vtable pointer, name pointer, flags; `dynamic_cast` and `typeid` runtime helpers.
- Exceptions: table-driven (A8e). Each function has an exception descriptor with call-site ranges, landing pads, cleanup actions, and type filters. A personality routine selects handlers; an unwinder walks frames and restores registers. `__cxa_throw`, `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_rethrow`, `__cxa_allocate_exception`, and `__cxa_free_exception` are runtime entry points.
- Cross-TU EH metadata is merged by the linker into a single table.
- `-fno-exceptions` and `-fno-rtti` profiles are supported for targets/programs that do not use them; the standard library subset must degrade to abort/terminate.
- The exact descriptor encoding is defined in `docs/abi.md` before v1.1. KR32’s existing `.cpp_except` is a reference, not a frozen format.

### 10.5 Varargs

- Variadic arguments are stack-passed in the target’s stack slot size; `float` promotes to `double`.
- `va_list` is a pointer to the first variadic slot (KR32) or an equivalent target structure.
- `va_start`, `va_arg`, `va_end`, `va_copy` are compiler builtins plus runtime support.
- `printf` and the runtime consume the same layout.

### 10.6 TLS and thread control block

- Static TLS in v1.0; dynamic TLS in v1.1.
- KR32: R11 points at the TCB; TLS offsets are link-time constants; the TCB contains the TLS block, stack limits, thread ID, and self pointer.
- KR16: no TLS in v1.0.
- `thread_local` objects with dynamic initialization require guards and destructor registration; gated to v1.1.
- The TCB layout is frozen in `docs/abi.md` before codegen depends on it.

### 10.7 Guards and static initialization

- Function-local statics use `__cxa_guard_acquire`/`release`/`abort` with a one-byte guard and a completion flag.
- Static constructors are registered in `.init_array`; destructors in `.fini_array` and/or `__cxa_atexit`.
- The linker provides `__init_array_start/end` and `__fini_array_start/end`.
- Construction order is translation-unit order within a link; destruction is reverse order.
- Cross-TU ordering is documented, not guaranteed beyond the standard’s rules.

### 10.8 C interoperability and attributes

- `extern "C"`, C headers, and C ABI calls are supported.
- `[[krc::interrupt]]`/`__attribute__((interrupt))`, `naked`, `noreturn`, `section`, `aligned`, and custom calling conventions are defined in `docs/attributes.md`.
- Attribute ABI effects (saved registers, return instructions, stack alignment) are target-specific and tested.

### 10.9 ABI versioning

- `ABI_VERSION` is a monotonic integer embedded in objects and the runtime.
- The compiler and runtime MUST agree on the ABI version; the linker rejects mismatches.
- ABI changes require an ADR, a version bump, and a runtime rebuild.
- No cross-version binary compatibility is promised (D-011).

---

## 11. Object format

The object format is a new, target-tagged container (D-013). Exact byte offsets are normative in `docs/object.md`; this section defines the required content.

### 11.1 Header

| Field | Width | Notes |
|---|---:|---|
| Magic | 4 | `KRO\0` |
| Version | 2 | major/minor |
| Kind | 2 | object, executable, library/archive member |
| Header size | 2 | |
| Flags | 2 | symbols/debug/stripped/relocatable |
| Target ID | 4 | stable numeric ID from the target descriptor |
| ABI version | 4 | |
| Pointer width | 1 | 16/32/64 |
| Endianness | 1 | 0 little, 1 big |
| Reserved | 2 | zero |
| Section count | 4 | |
| Symbol count | 4 | |
| Relocation count | 4 | |
| String table offset/size | 8/8 | |
| Debug table offset/size | 8/8 | |
| Entry address | 8 | zero for objects |
| Image base | 8 | zero for objects |
| File size | 8 | |
| Build ID | 16 | |
| CRC32 | 4 | covers the file with this field zeroed |
| Reserved | 8 | zero |

### 11.2 Sections

| Field | Width |
|---|---:|
| Name offset | 4 |
| Type | 2 |
| Flags | 2 |
| Address | 8 |
| Payload offset | 8 |
| Size | 8 |
| Alignment | 4 |
| Link/info | 4 |
| Entry size | 4 |
| Reserved | 8 |

Section types: text, rodata, data, bss, init_array, fini_array, tls, debug, note, other. Flags: alloc, write, execute, TLS, merge, strings, discardable.

### 11.3 Symbols

| Field | Width |
|---|---:|
| Name offset | 4 |
| Value | 8 |
| Size | 8 |
| Section index | 4 |
| Binding | 1 (local/global/weak) |
| Type | 1 (notype/object/function/section/file/tls) |
| Visibility | 1 |
| Flags | 1 (COMDAT, constructor, etc.) |
| Target-specific | 4 |
| Reserved | 4 |

### 11.4 Relocations

| Field | Width |
|---|---:|
| Section index | 4 |
| Offset | 8 |
| Type | 4 |
| Symbol index | 4 |
| Addend | 8 |
| Width | 1 |
| Flags | 3 |

Common relocation families: absolute 8/16/32/64, PC-relative 8/16/32/64, paired hi/lo, GOT/PLT reserved (unused), target-specific ranges. The linker MUST reject overflow and MUST apply target-specific relaxation rules.

### 11.5 Validation and determinism

- Magic, version, header size, target ID, ABI version, pointer width, and endianness are validated on read.
- Section/symbol/relocation tables are bounds-checked; string offsets are validated.
- CRC32 covers the whole file with the CRC field zeroed.
- Symbols are written in a deterministic order: globals by name, locals by section/offset, then strings in first-use order.
- No timestamps or absolute build paths in the object; debug source paths are remapped to a build root.

### 11.6 Executable images

- The linker produces a common linked model, then a per-target image writer emits the VM’s loadable image.
- KR32 and KR16 image writers initially emit their native formats (or a documented compatible variant); the common container is the object/link format.
- The image writer receives the linked sections, entry point, symbols/debug, and target layout. It MUST reject unsupported section placements or address ranges.
- The long-term option is one common image format loaded by all VMs; that requires VM loader changes and is not a v1.0 requirement.

---

## 12. Linker

### 12.1 Requirements

- Link object files and archives for any supported target.
- Validate target ID, ABI version, pointer width, and endianness.
- Resolve global/weak symbols; merge COMDAT/vague-linkage groups (templates, inline functions, vtables, typeinfo).
- Apply relocations with overflow diagnostics and target relaxation (branch shortening/lengthening).
- Garbage-collect unused sections when requested.
- Merge debug sections and preserve source/line/type/variable records.
- Produce a map file and size report.
- Support a small layout script for memory regions and section placement.
- Strip symbols/debug on request.
- Produce deterministic output.

### 12.2 Link model

1. Read objects/archives; validate headers and target tags.
2. Build the global symbol table; resolve strong/weak definitions; report duplicates and unresolved symbols.
3. Select archive members only when needed (unless `--whole-archive`).
4. Merge sections by name/flags/alignment; handle COMDAT groups.
5. Assign addresses according to the target layout script/default.
6. Apply relocations; relax branches; iterate until stable.
7. Garbage-collect if requested; recompute addresses.
8. Merge debug info; build the symbol/debug container.
9. Write the image via the target writer; write optional sidecar debug; write map/size reports.

### 12.3 Layout scripts

A minimal `krld` layout file (not GNU ld syntax) specifies:

```text
target kr32
region rom  0x00000000 size 0x00100000
region ram  0x00100000 size 0x00300000
section .text  > rom
section .rodata > rom
section .data  > ram
section .bss   > ram
entry _start
```

Target defaults are built in; user scripts override placement only. Unknown directives are errors, not ignored.

### 12.4 Linker CLI

```text
krld [options] objects... -o image
  --target=NAME
  --script=FILE
  --map=FILE
  --size
  --gc-sections
  --strip
  --debug-file=FILE
  --whole-archive
  --library-path=DIR
  --library=NAME
  --verbose
```

### 12.5 Out of scope

PIC/PIE, shared libraries, dynamic linking, lazy binding, and LTO are out of scope for v1 (D-014). The object/relocation model reserves fields for them, but no implementation is required.

---

## 13. Symbol and debug format

### 13.1 Design goals

- One format for all targets and all debuggers.
- 64-bit canonical addresses/offsets, independent of target pointer width.
- Rich enough for C++ classes, templates (reserved), scopes, threads, and optimised locations.
- Compact enough for 16-bit images.
- Robust: versioned, checksummed, bounds-checked, build-ID/source-hash validated.
- Human-inspectable via a canonical text/JSON dumper.

### 13.2 Container sections

| Section | Contents |
|---|---|
| `.kdbg_header` | Magic `KDBG`, version, target ID, ABI version, pointer width, endianness, build ID, source count, string table, CRC. |
| `.kdbg_src` | Source file records: path, build directory, language, checksum. |
| `.kdbg_line` | Line table: address ranges -> file/line/column; multiple ranges per function. |
| `.kdbg_func` | Functions: source/mangled name, ranges, frame size, return type, params, class/`this`, flags. |
| `.kdbg_frame` | Frame/unwind records: saved FP/SP, return address, CFA rules, register restore. |
| `.kdbg_sym` | Link-visible symbols: name, value, size, section, binding, type, visibility. |
| `.kdbg_type` | Type table: builtins, pointers, arrays, structs/classes/unions/enums, typedefs, functions, references, member pointers, qualifiers, template placeholders. |
| `.kdbg_var` | Variables: name, type, location, scope, declaration line, flags (param/local/static/global/TLS). |
| `.kdbg_scope` | Namespaces, classes, functions, lexical blocks; parent and ranges. |
| `.kdbg_str` | Deduplicated string table. |
| `.kdbg_build` | Compiler version, build ID, target features, source hashes, ABI version. |

### 13.3 Record model

- Records use a tag + length + payload encoding; unknown tags are skipped by readers.
- Addresses and offsets are 64-bit canonical values.
- Types are referenced by stable IDs; strings by offsets.
- Functions own line ranges and variable/scope lists; scopes may nest.
- Frames describe how to unwind and where saved registers live.
- Variables carry a location expression (see §13.4).
- Threads are a runtime/DAP concept; TLS variables and per-thread frame rules are in the symbol format.

### 13.4 Location expressions

A compact stack-machine expression describes a variable’s location:

- `addr` (absolute), `frame_offset`, `register`, `tls_offset`, `deref`, `add`, `sub`, `piece`, `const`.
- v1.0 emits `frame_offset` and `register` for `-O0`; location lists (address ranges -> expression) are reserved for `-O1` and later.
- The debugger evaluates expressions for `p->x`, `a[i]`, casts, and simple arithmetic.

### 13.5 Debugger feature mapping

| Feature | v1.0 | Notes |
|---|---|---|
| Breakpoints by file/line | yes | Line table + function ranges. |
| Call stack/frame unwind | yes | Frame records + CFA rules. |
| Locals/params/globals/statics | yes | Variable records. |
| Struct/class/array/pointer expansion | yes | Type table + member offsets. |
| Inheritance/virtual/dynamic type | yes | Class layout + vtable/RTTI links. |
| Expression evaluation/watches | yes | Typed evaluator over locations/types. |
| Threads/cores | yes | Runtime/DAP thread query; per-thread frames. |
| Memory/disassembly mapping | yes | Address -> line/function. |
| Template display names | later | Type/function names may show mangled forms initially. |
| Inline frames | later | Reserved records; `-O1` line/function only. |
| Optimised variable locations | later | Location lists reserved. |

### 13.6 Sidecar and stripping

- Debug info is in the image by default.
- `krld --debug-file=FILE` writes a sidecar; the image carries a build ID and a flag, and the debugger locates the sidecar by build ID.
- `--strip` removes debug sections and local symbols; the build ID remains for matching.
- A sidecar is the preferred shipping form for small KR16 images.

### 13.7 Versioning and robustness

- Major/minor version; readers accept the same major and newer minor.
- Unknown record types are skipped; unknown required flags are errors.
- CRC32 and bounds checks on every table.
- Build ID and source hashes detect stale/mismatched debug info; the debugger warns rather than silently showing wrong lines.
- String table offsets are validated; duplicate/missing strings are diagnostics.
- The dumper (`krc --dump-symbols`) emits canonical text and JSON; the JSON schema is stable for tooling.

### 13.8 Shared reader library

`libkrcdebug` provides:

- container validation and loading;
- source/line lookup;
- function/frame/scope/type/variable queries;
- location-expression evaluation primitives;
- thread/TLS metadata;
- a stable C++ API for the GUI and DAP server.

All debuggers MUST use this library; no debugger parses the format directly.

---

## 14. Runtime and libraries

### 14.1 Architecture

- `krsrc/common/` — portable C/C++ runtime, HAL interfaces, allocator, stdio, string, math helpers, startup logic.
- `krsrc/<target>/` — crt0, syscall stubs, soft-float/64-bit helpers, unwinder, context switch, target-specific `memcpy`/`memset` fast paths.
- `sysroot/<target>/` — headers and prebuilt libraries.
- The runtime is compiled by `krc` for each target; a C-only bootstrap path is available for the first bring-up.

### 14.2 Startup

- `_start`/crt0 sets up the stack, globals, heap bounds, TLS, and the C environment.
- It runs `.init_array` constructors, calls `main(argc, argv)`, and exits through `exit`.
- `argc/argv` come from the host/BIOS command line if available; otherwise `argc = 0`, `argv = NULL`.
- No environment variables in v1; `environ` is not provided.
- Destructors run via `.fini_array`/`__cxa_atexit`; exit codes propagate to the VM.

### 14.3 C library surface

The v1.0 sysroot provides the subset used by the corpus:

| Header | Functions |
|---|---|
| `assert.h` | `assert` (via `__krc_assert_fail`) |
| `ctype.h` | `is*`, `to*` |
| `errno.h` | `errno` |
| `float.h`, `limits.h`, `stdint.h`, `stddef.h` | generated target limits and typedefs |
| `math.h` | `fabs`, `fmin`, `fmax`, `sqrt` on KR32; no FP on KR16 |
| `stdarg.h` | `va_list`, `va_start`, `va_arg`, `va_end`, `va_copy` |
| `stdbool.h`, `stdalign.h`, `stdnoreturn.h` | macros |
| `stdio.h` | `FILE`, `fopen`/`fclose`/`fread`/`fwrite`/`fseek`/`ftell`/`fgets`/`getline`/`ungetc`/`setvbuf`, `printf`/`fprintf`/`snprintf`/`sprintf`/`vprintf`, `scanf`/`sscanf`, `putchar`/`puts`/`getchar` |
| `stdlib.h` | `malloc`/`free`/`calloc`/`realloc`, `exit`/`abort`, `atoi`/`atol`/`strtol`/`strtoll`/`strtod`, `qsort`/`bsearch`, `abs`/`labs`, `getenv` (returns null) |
| `string.h` | `memcpy`/`memmove`/`memset`/`memcmp`, `strlen`/`strcpy`/`strncpy`/`strcat`/`strncat`/`strcmp`/`strncmp`/`strchr`/`strrchr`/`strstr`/`strtok`/`strdup`/`strerror` |
| `time.h` | `time`, `difftime`, `clock` (host-service backed) |
| POSIX-ish | `open`/`close`/`read`/`write`/`stat`/`getcwd`/`chdir` (host-service backed) |

- `printf` implements standard length modifiers and `%p`; floating-point formatting on KR32 using the runtime’s float path; KR16 has integer/string/char conversions only.
- The allocator is a coalescing free-list allocator with linker-provided heap bounds; alignment follows the target.
- `malloc(0)`, `realloc`, and overflow checks are defined and tested.
- `_heapwalk` is not part of the standard surface; it is a debug-only extension guarded by `KR_DEBUG_HEAP`.

### 14.4 C++ runtime

- `operator new`/`delete`, array forms, nothrow forms, placement forms.
- `__cxa_*` entry points: `__cxa_atexit`, `__cxa_guard_*`, `__cxa_throw`, `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_rethrow`, `__cxa_allocate_exception`, `__cxa_free_exception`, `__cxa_call_unexpected`, `std::terminate`/`std::unexpected`.
- `type_info`/`typeid`/`dynamic_cast` helpers.
- Exception unwinder: target-specific frame walking behind a shared interface.
- `std::exception`, `std::bad_alloc`, `std::bad_cast`, `std::bad_typeid` (v1.1).
- Static initialization guards and atexit registration.

### 14.5 Standard library ladder

| Tier | Contents |
|---|---|
| v1.0 | Freestanding headers: `type_traits` (basic), `limits`, `new`, `exception`, `initializer_list`, `cstddef`, `cstdint`, `utility` (move/forward/pair basics). |
| v1.1 | `memory` (`unique_ptr`, `shared_ptr`), `tuple` basics, `functional` basics, `array`. |
| v1.2 | `string`, `vector`, `algorithm`. |
| v1.3 | `iostream`/`stringstream`, `map`/`unordered_map`/`deque`/`list`. |
| Later | C++20 library, C++23, ranges, coroutines, filesystem, regex, locale. |

No `std::thread` surface in v1 (D-022). Guest threads, mutexes, condition variables, and atomics are a target runtime surface (`kr32/thread.hpp`) for kernel/runtime use; exposing `std::thread` is a later decision.

### 14.6 Host services

A common host-service ABI, implemented per VM:

| Service | Purpose |
|---|---|
| `console_write`/`console_read` | Character I/O |
| `exit` | Terminate with a code |
| `heap_info` | Heap start/end/limit |
| `file_open`/`close`/`read`/`write`/`seek`/`stat` | Sandboxed file I/O |
| `time` | Monotonic/wall clock |
| `random` | Deterministic/random bytes |
| `thread_create`/`join`/`yield`/`exit` | Guest threads (capability-gated) |
| `futex_wait`/`wake` | Blocking primitives |
| `debug_break` | Breakpoint trap |

Per-VM shims map the ABI to `kr32os` syscalls, KR16 `SYS`/`TRAP`, or a host test shim.

### 14.7 Soft-float and 64-bit helpers

- Portable C implementations of 64-bit multiply/divide/shift/compare, soft-float `float`/`double` operations, and conversions.
- Target-specific overrides where hardware exists (KR32 FPU).
- KR16 IP16 profile omits 64-bit integers and FP; helpers are not built for that profile.
- All helpers are tested against host `long long`/`double` for the supported range.

### 14.8 Distribution and bootstrap

- The runtime is prebuilt per target per compiler release, versioned by ABI and compiler version.
- Source is shipped for rebuilds; a `runtime` build target compiles it with `krc`.
- Bring-up order: minimal C runtime → C++ runtime → EH/RTTI → standard library.
- The runtime MUST NOT depend on host headers or host libraries.
- The compiler is host-only; self-hosting is not a goal (D-033).

---

## 15. Driver, tooling, repository, and process

### 15.1 `krc` CLI

```text
krc [options] files...
  -std=c11|c++11|c++14|c++17|c++20
  --target=kr32|kr16|kr64
  -E -S -c -o FILE
  -g -O0 -O1
  -I DIR -isystem DIR -D MACRO -U MACRO
  -L DIR -l NAME
  --sysroot=DIR -nostdinc
  --dump-ast --dump-ir --dump-asm --dump-symbols
  --emit-json
  -MD -MF FILE
  @response-file
  --version --help
```

- Default dialect: `.c` → `c11`; `.cpp` → `c++11` until `c++17` is stable.
- Default output: link to an image when `-o` is given without `-c`/`-S`; assembly to stdout with `-S`; object with `-c`.
- `krc` invokes the integrated assembler/linker; `krld` remains available for standalone linking.
- Diagnostics use a stable format with file:line:column, severity, message, notes, and optional fix-it.

### 15.2 `krld` CLI

See §12.4. The linker is usable standalone and through `krc`.

### 15.3 Repository layout

```text
plan.md
implementation.md
LICENSE
CMakeLists.txt
CMakePresets.json
cmake/
docs/
  abi.md
  abi-kr16.md
  attributes.md
  ir.md
  object.md
  symbols.md
  runtime.md
  testing.md
  targets/kr32.md
  targets/kr16.md
  targets/kr64.md
  decisions/
include/krc/
src/
  frontend/
  ir/
  backend/
  asm/
  linker/
  debug/
  runtime/
  tools/
sysroot/
tests/
  unit/
  integration/
  differential/
  conformance/
  golden/
  corpus/
tools/
examples/
```

### 15.4 Build and CI

- CMake 3.25+, C++20, no external runtime dependencies; doctest vendored for tests.
- Windows MSVC and Linux GCC/Clang in CI; warnings-as-errors (`/W4 /WX`, `-Wall -Wextra -Wpedantic -Werror`).
- Sanitizers (ASan/UBSan) on Linux.
- Deterministic-output check: build the same inputs twice and compare bytes.
- Golden IR/assembly/symbol tests and differential tests run in CI.
- ARM64 host CI is later; the compiler must remain portable C++20.

### 15.5 Testing

- Unit tests for every front-end/IR/backend component.
- Integration tests: compile → assemble → link → run on the VM.
- Differential tests: host vs VM for the portable corpus.
- Extracted fixtures: C++ feature coverage.
- Curated conformance: C11/C++11/14/17 feature tests.
- Golden tests: IR, assembly, object headers, symbol dumps.
- Negative tests: unsupported features, target-profile violations, ABI/target mismatches.
- Debugger tests: DAP protocol and GUI-level sessions.
- Fuzzing: preprocessor/lexer/parser later.

### 15.6 Determinism and versioning

- Deterministic outputs: sorted symbols, stable section order, no timestamps, build-root remapping.
- Separate versions: compiler, language mode, ABI, object format, debug format, target descriptor.
- A compatibility matrix is maintained in `docs/versions.md`.
- Build IDs and source hashes connect images, debug sidecars, and sources.

### 15.7 Documentation and process

- `plan.md` (this document) is the design contract.
- `implementation.md` is the live milestone/task tracker.
- ADRs under `docs/decisions/` for ABI, object, symbol, target, and IR changes.
- Handoff notes for each milestone, following KR32/KR16 conventions.
- Every merged change updates the relevant docs and the implementation log.
- License: MIT.

---

## 16. Milestones and releases

### 16.1 Milestone plan

Durations assume one to two full-time-equivalent engineers/agents. They overlap where dependencies allow.

| Milestone | Duration | Deliverables | Gate |
|---|---:|---|---|
| **M0 Foundation** | 2–3 weeks | Repo, CMake, CI, docs skeleton, source manager/diagnostics, target descriptor skeleton, fixture extractor, decision log. | G0 |
| **M1 C front end + sysroot** | 6–8 weeks | Preprocessor, lexer, C11 parser, Sema core, real sysroot headers, AST dump. | G1 |
| **M2 IR + KR32 backend + assembler** | 8–10 weeks | Typed SSA, C IRGen, KR32 descriptor, instruction selection, regalloc, frames, asm printer, assembler. | G2a |
| **M3 Object + linker + startup** | 6–8 weeks | Object format, `krld`, crt0, minimal libc, KR32 image writer. | G2b |
| **M4 C core + debug format** | 8–10 weeks | Structs/pointers/arrays/varargs/preprocessor, KDBG format, dumper/reader, DAP/GUI debug session. | G3, G4 |
| **M5 C++ core** | 8–10 weeks | Classes, single inheritance, virtual, references, overloading, ctors/dtors, `new`/`delete`, namespaces. | G5 |
| **M6 C++11/14/17** | 8–10 weeks | Templates, lambdas, move, `auto`/`decltype`, range-for, `constexpr`, `initializer_list`, `noexcept`/`alignas`, `thread_local` static, `if constexpr`, structured bindings, inline variables; fixtures. | G6 |
| **M7 KR16 backend + C** | 6–8 weeks | KR16 descriptor, IP16 data model/ABI, codegen, image writer; Tier 0–1 C. | G7 |
| **M8 v1.0 hardening** | 6–8 weeks | Full Tier 0–2, debugger parity, determinism, docs, tag `v1.0`. | G8 |
| **M9 v1.1 runtime/libc/host services** | 8–10 weeks | Port KR32 `libc.c`, stdio/`printf`, malloc, file I/O, host ABI, kernel profile; integrate KR16 `libc.s`/`fileio.c`; Tier 3. | G9 partial |
| **M10 v1.1 language/runtime completion** | 8–10 weeks | EH/RTTI, variadic templates + fold expressions, dynamic TLS, utilities/memory, full KR32 `libc.c`/`kr32os`, KR16 runtime; tag `v1.1`. | G9 |

Sequential execution of these durations is roughly 14–17 months to v1.0. The 12-month target assumes parallel workstreams (front end/IR, linker/debug, runtime/sysroot) and agent assistance. The 6–9 month usable milestone is M5/M6.

### 16.2 Release definitions

- **v0.1 (month 3–4):** C on KR32, hello/mainentry, object/link, minimal runtime.
- **v0.5 (month 6–8):** C core + debugger on KR32; C++ core begins.
- **v1.0 (month 12 target; 12–14 realistic with sequential work):** KR32 C++ core + selected C++14/17 + debugger; KR16 C; Tier 0–2; minimal Tier 3.
- **v1.1 (month 12–18):** EH/RTTI, variadic templates, dynamic TLS, utilities/memory, full KR32 `libc.c`/`kr32os`, KR16 runtime.
- **v1.2:** `string`/`vector`/`algorithm`, multiple/virtual inheritance, C++20 language essentials.
- **v1.3:** `iostream` + `map`/`unordered_map`.
- **Later:** C++20 library, C++23, 64-bit target, stack/8-bit targets, SFINAE/two-phase/concepts.

### 16.3 Critical path

1. Sysroot + preprocessor + C parser/Sema.
2. IR + KR32 backend + object/linker.
3. Debug format + reader + debugger integration.
4. C++ classes/templates.
5. KR16 backend.
6. Runtime/libc/kr32os.

The C++ front end and the runtime/libc port are the two largest work packages. The v1.0 boundary deliberately keeps EH/RTTI, variadic templates, dynamic TLS, and full KR32 `libc.c`/`kr32os` (plus KR16 runtime integration) in v1.1 to protect the 12-month target.

---

## 17. Risks and open questions

### 17.1 Risk register

| Risk | Impact | Mitigation |
|---|---|---|
| No sysroot; synthesised headers | High: every real program fails | Make the sysroot an M1 deliverable; generated `bits/`; no host headers. |
| C++ front end scope | High: schedule | Stage features; reject unsupported constructs with diagnostics; fixture-driven. |
| No standalone conformance corpus | High: silent regressions | Build the extractor in M0/M1; curated suite; golden tests. |
| C++ target corpus is six example files, three substantial | High: weak coverage | Extract fixtures; add new tests per feature. |
| `libc.c`/`kr32os` port | High: v1.1 slip | Port incrementally; keep v1.0 boundary at Tier 2; use `libc.c` as reference. |
| KR16 16-bit profile | Medium: non-conforming code surprises | Document profile; audit `long`; lo/hi structs; diagnostics. |
| MMIO without `volatile` | High: kr32os breaks under optimisation | Corpus migration; compiler honours `volatile`; tests. |
| Non-standard `printf`/APIs | Medium: corpus mismatch | Standardise `printf`; migrate `%p`/length modifiers; profile APIs. |
| Non-conforming C (`void main`, implicit decls) | Medium: strict compiler rejects corpus | Corpus cleanup; diagnostics; migration list. |
| EH/RTTI in v1.1 | High: custom ABI/runtime | Design ABI now; implement behind tables; test cross-TU. |
| Template instantiation/COMDAT | High: duplicate symbols | Linker weak/COMDAT from M3; test multi-TU templates. |
| ABI churn | High: rework | Freeze ABI before M5; conformance suite; versioning. |
| Debug format too weak for C++ | High: debugger gaps | Design type/scope records now; shared reader; fixtures. |
| Optimised debug expectations | Medium | `-O0` full; `-O1` line/function; location lists reserved. |
| 64-bit target undefined | Low v1, high later | Width-independent IR/ABI; placeholder; separate VM spec. |
| Schedule | High | v1.0 boundary; vertical slices; v1.1 for EH/RTTI/libc. |

### 17.2 Open questions

1. **64-bit target identity and ISA.** A new VM, a KR63 variant, or a KR32 extension? The compiler plan assumes a new spec, but this needs its own project decision.
2. **KR16 C++.** Is C++ ever required on KR16, or is it C-only permanently?
3. **Target descriptor format.** C++ `constexpr` tables are the v1 choice; is a data-file format required later?
4. **KR16 ABI details.** Argument/return/aggregate/varargs/`this` rules need a dedicated appendix and tests.
5. **EH record encoding.** KR32’s `.cpp_except` is a reference, not final; the v1.1 format needs an ADR.
6. **`libc.c` strategy.** Port it as the base, or write a new runtime and use `libc.c` only as a test? The plan assumes port-as-base.
7. **`printf` compatibility.** Standard-only, or a compatibility mode for the existing corpus’s `%h` pointer idiom?
8. **Corpus cleanup ownership.** Who fixes `volatile` MMIO, `void main`, implicit declarations, and non-standard APIs?
9. **`packed`/`typeof`/GNU extensions.** Needed by fixtures but not selected; confirm they stay out of v1.
10. **`std::thread`.** Guest threads are in; is the `std` threading surface ever required?
11. **C++20 concepts/SFINAE.** Confirm the long-term order: SFINAE/two-phase first, concepts after.
12. **Matrix/TLS/thread examples.** Are they v1.1 tests or opt-in extras?
13. **Linker script syntax.** Is the proposed minimal `krld` layout format acceptable, or is GNU ld compatibility wanted?
14. **Symbol format text/JSON schema.** Confirm the canonical dump is the tooling contract.
15. **Host ARM64 CI.** Required at v1.0 or later?
16. **Third-party test suites.** Are c-testsuite/libc++ tests acceptable as test-only dependencies, given the “own tools” goal?

---

## 18. Approval

### 18.1 Approval checklist

- [x] Goals, non-goals, and success criteria (§2) are accepted.
- [x] Target matrix and data models (§4) are accepted, including the non-conforming KR16 profile.
- [x] Corpus tiers, sysroot requirement, and acceptance gates (§5) are accepted.
- [x] C and C++ language scope and staging (§6) are accepted.
- [x] Compiler architecture and IR contract (§7–§8) are accepted.
- [x] Backend/target-description approach (§9) is accepted.
- [x] ABI direction (§10), object format (§11), linker (§12), symbol/debug format (§13) are accepted.
- [x] Runtime and library ladder (§14) are accepted.
- [x] Tooling, repository, testing, and determinism (§15) are accepted.
- [x] Milestones, v1.0/v1.1 boundaries, and schedule (§16) are accepted.
- [x] Risks and open questions (§17) are accepted as tracked items.

### 18.2 After approval

1. Create `implementation.md` from §16 with task checklists and a running log.
2. Create `docs/abi.md`, `docs/object.md`, `docs/symbols.md`, `docs/ir.md`, `docs/attributes.md`, and target profiles.
3. Start M0 (repository, build, CI, source manager/diagnostics, fixture extractor).
4. Freeze the target-descriptor schema and the C ABI before M2/M5.
5. Review this plan at each milestone gate; update the decision log for any change.

### 18.3 Sign-off

- Project owner: Kevin  Date: 2026-11-09
- Compiler lead: Kevin (project owner; acting compiler lead)  Date: 2026-11-09
- Notes / conditions: All eleven §18.1 items approved without conditions. Review findings are recorded in `docs/planning/nemo-plan-review.md`.

---

## Appendix A — Decision log

### A.1 Survey decisions

| ID | Decision | Answer |
|---|---|---|
| R1 | Primary deliverable | All four goals, phased. |
| R1b | v1 priority | Usable VM toolchain plus clean architecture. |
| R2 | First corpus | Demos + libc + OS. |
| R2b | Corpus source | Derive from repos and propose a list; include the experimental C++ tests as feature fixtures. |
| R3 | Target order | KR32 → KR16 → 64-bit. |
| R3b | Tool shape | One `krc` + `krld`, `--target` selects the backend. |
| R3c | ISA changes | Frozen by default; small versioned additions allowed. |
| R3d | 64-bit target | Not decided; plan for width independence. |
| R4 | C++ acceptance | Documented subset + clear diagnostics. |
| R4b | C11 acceptance | C11 core minus optional/obscure features. |
| R4c | Subset enforcement | Feature matrix + target capability profiles. |
| R5 | C++23 meaning | C++11→17 core + selected 20/23. |
| R6 | Plan ambition | 6–12 month usable v1, then iterate. |
| R6b | v1 content | KR32: C11 + C++ classes/templates + debugger; KR16: C subset; EH/RTTI/std in v1.1. |
| R7 | Migration | Coexist, then cut over; clean object break. |
| R8 | Audience/process | Personal/teaching; MIT; KR32/KR16 conventions. |
| R8b | Existing spec | Use KR32/KR16 docs as input. |
| A1 | Dialect architecture | Shared infrastructure; dialect-aware Sema; separate parse paths. |
| A2 | IR level | Typed SSA in memory + textual dump. |
| A2b | IR contents | EH/cleanup, volatile/MMIO, atomics, debug locations, vtable/RTTI, source types. |
| A3 | Backend | Hybrid shared framework + target descriptions/hooks. |
| A3b | Output | Assembly text default; objects via shared writer. |
| A3c | Assembler | One framework + per-target syntax/encoding modules. |
| A4 | ABI | One versioned ABI, KR32 base; redesign `.cpp_except`. |
| A4b | ABI stability | Tied to compiler version; runtime rebuilt each release. |
| A5 | Data model | Strict 32/64; IP16 embedded profile on KR16. |
| A5b | 64-bit model | LP64. |
| A5c | Plain char | Per-target, documented with `__CHAR_UNSIGNED__`. |
| A6 | Object format | New target-tagged container, 64-bit fields. |
| A6b | Linker features | Static objects/archives, weak/COMDAT, GC, relaxation, map/size, scripts, debug merge/strip. |
| A6c | Images | One linker core + per-target image writers initially. |
| A7 | Symbol/debug format | One versioned binary format + canonical dumper. |
| A7b | Debug features | Breakpoints, stack, variables, aggregates, inheritance/virtual/dynamic type, expressions/watches, threads, memory/disassembly. |
| A7c | Optimised debug | `-O0` full; `-O1` line/function. |
| A7d | Debug storage | In-image + optional sidecar. |
| A8 | Runtime | Common source + HAL + per-target stubs. |
| A8b | C++ library | Freestanding v1; utilities/memory v1.1; string/vector/algorithm; iostream + map/unordered. |
| A8c | Host services | Common ABI + per-VM shims. |
| A8d | C library | Minimal freestanding C library. |
| A8e | EH/RTTI | Table-driven EH + Itanium-like RTTI, capability-gated. |
| A8f | Library phasing | v1 freestanding → v1.1 core+memory → v1.2 string/vector → v1.3 iostream + map/unordered. |
| A8g | Runtime threading | Guest threads/atomics/guards; no `std::thread` in v1. |
| A9 | Tool/repo | One `krc` + `krld` + shared parser library; monorepo. |
| A9b | Build/CI | CMake + C++20 + doctest; Windows + Linux; deterministic. |
| A9c | Test strategy | Differential + curated conformance + golden dumps. |
| L0 | C++11 floor | Classes/inheritance/virtual, templates, lambdas, move, auto/decltype/range-for/nullptr/enum class, constexpr, initializer_list, noexcept/alignas/thread_local. Variadic templates are staged v1.1; multiple/virtual inheritance is staged v1.2. |
| L1 | C++14/17 | C++14 included; C++17 core (`if constexpr`, structured bindings, fold expressions, inline variables). Fold expressions are staged with variadic templates in v1.1. |
| L2 | C++20/23 | C++20 language essentials; C++23 later. |
| L3 | C11 | C11 conveniences; C99 designated/compound added by S-G3. |
| L4 | Target extensions | Matrix/transform, atomics/LL-SC/fence, syscalls, volatile/MMIO. |
| S-G1 | C++14 baseline | Yes, part of the C++17 baseline. |
| S-G2 | Concepts/templates | Concepts later; SFINAE/two-phase first. |
| S-G3 | C initializers | Designated initializers + compound literals. |
| S-G5 | OS attributes | Interrupt, naked, noreturn, custom conventions. |

Survey decision IDs use the `S-G` prefix to avoid collision with acceptance gates G0, G1, G2a, G2b, and G3–G9. `S-G4` was not asked.

### A.2 Architecture decisions

| ID | Decision | Answer | Rationale |
|---|---|---|---|
| D-001 | Dialects | One driver, two dialects; shared source manager, preprocessor, lexer, AST, and IR; dialect-aware Sema; separate C and C++ parse paths. | C11 is not a subset of C++11; the dialects need distinct semantics. |
| D-002 | Targets | KR32 is the v1 reference; KR16 is the v1 C target; the 64-bit target is a placeholder; stack/8-bit targets are out. | KR32 is the most complete target; KR16 forces width independence; stack/8-bit need a separate backend family. |
| D-003 | Tool shape | `krc` + `krld` + shared parser/reader libraries; monorepo; `--target` selects the backend. | One toolchain and one linker; per-target binaries duplicate work. |
| D-004 | ISA policy | ISAs are frozen by default; small, versioned additions are allowed with a feature flag, docs, VM change, and golden vectors. | Protects VM stability while allowing unwinder/atomic needs. |
| D-005 | IR | Typed SSA in memory plus a textual dump; no stable serialised IR. | Enough for optimisation and retargeting without a second compatibility product. |
| D-006 | IR contents | Explicit EH/cleanup, volatile/MMIO, atomics, debug locations, vtable/RTTI, and source types; no address spaces/banking or inline metadata. | Needed for C++ and these VMs; out-of-scope features are deferred. |
| D-007 | Backend | Hybrid shared framework plus C++ `constexpr` target descriptions and hooks. | Scales across targets without a generator or external dependency. |
| D-008 | Output | Assembly text by default; objects through a shared writer; `-S`/`-c` share encoding tables. | Inspectable and teachable, with an efficient object path. |
| D-009 | Assembler | One framework plus per-target syntax/encoding modules. | Shared diagnostics/object writer; preserves target syntax. |
| D-010 | ABI | One versioned C/C++ ABI, KR32 base; redesigned `.cpp_except`; pointer-width/endianness parameters. | Common runtime/debug; KR32 proven; custom EH is not final. |
| D-011 | ABI stability | ABI tied to compiler version; runtime rebuilt each release. | Avoids premature ABI freeze and simplifies evolution. |
| D-012 | Data models | Strict 32/64-bit; IP16 profile on KR16; LP64 future; per-target `char` with `__CHAR_UNSIGNED__`. | Conformance where practical; honest 16-bit profile; existing VM behaviour. |
| D-013 | Object format | New target-tagged container with 64-bit canonical fields. | Existing containers cannot represent 64-bit targets; one format for all. |
| D-014 | Linker | Static objects/archives, weak/COMDAT, GC, overflow checks, relaxation, map/size, layout scripts, debug merge/strip; no PIC/dynamic/LTO. | Covers the corpus without dynamic-linking complexity. |
| D-015 | Images | One linker core plus per-target image writers initially. | Avoids rewriting every VM loader. |
| D-016 | Symbols/debug | One binary versioned target-tagged format plus canonical dumper and shared reader. | Debugger-grade C++ info and one parser for all tools. |
| D-017 | Debug features | Breakpoints, stack, variables, aggregates, inheritance/virtual/dynamic type, expressions/watches, threads, memory/disassembly. | Matches the requested debugger surface; templates/inline frames later. |
| D-018 | Optimised debug | `-O0` full; `-O1` line/function. | Variable locations and inline frames are expensive; the format is reserved. |
| D-019 | Debug storage | In-image default; optional sidecar by build ID. | Auto-discovery plus small shipping images. |
| D-020 | Runtime | Common source + HAL + per-target crt/syscall/soft-float/unwinder stubs. | One runtime tree across targets. |
| D-021 | Library ladder | v1 freestanding; v1.1 utilities/memory; v1.2 string/vector/algorithm; v1.3 iostream + map/unordered. | Dependency order; each stage is usable. |
| D-022 | Threading | Guest threads/atomics/guards; no `std::thread` in v1. | Runtime needs threads/atomics; the `std` threading surface was not selected. |
| D-023 | C mode | C11 conveniences plus C99 designated/compound; no VLAs/complex/atomics/threads/K&R. | Corpus needs; optional/obscure features deferred. |
| D-024 | C++ ladder | C++11 core + C++14 + selected C++17 in v1.0; C++20 language essentials v1.2; concepts after SFINAE/two-phase; no C++20 library or C++23. | Practical modern C++ with a credible conformance path. |
| D-025 | Template machinery | SFINAE/two-phase post-v1; concepts after them. | Largest front-end cost; no corpus dependency. |
| D-026 | Inheritance | Single inheritance v1.0; multiple/virtual v1.2. | No corpus use; layout and dynamic-cast cost. |
| D-027 | Attributes | Basic standard attributes plus OS attributes. | Diagnostics and OS code at low cost. |
| D-028 | Corpus | Tier 0–2 KR32, Tier 0–1 KR16 in v1.0; Tier 3 v1.1; Tier 4 stretch. | Matches corpus size/dependencies and protects the schedule. |
| D-029 | Sysroot | Real headers, generated target `bits/`, no host headers. | A retargetable compiler needs target headers; synthesis is a structural gap. |
| D-030 | v1 boundary | v1.0 is a usable toolchain plus debugger; v1.1 adds EH/RTTI, variadic templates, dynamic TLS, utilities/memory, full KR32 `libc.c`/`kr32os` and KR16 runtime integration. | The 12-month target; those features are large. |
| D-031 | Migration | Coexist, then cut over; clean object break. | Old compilers remain usable; the new format needs no compatibility. |
| D-032 | Testing | Differential + curated conformance + golden dumps. | The corpus lacks a standalone conformance suite; multiple evidence layers. |
| D-033 | Self-hosting | Not a goal; host-only compiler. | Avoids hosted C++/runtime requirements. |
| D-034 | Process | MIT; KR32/KR16 conventions; ADRs. | Consistent with the existing projects. |
| D-035 | Determinism | Byte-identical outputs, sorted symbols, path remapping, build IDs and source hashes. | Reproducible tests/releases and debugger matching. |

Changing a `D-xxx` decision requires editing its row here and adding an ADR under `docs/decisions/` when the change touches ABI, object format, symbol format, target description, or IR.

---

## Appendix B — Corpus tiers and compile-first list

### B.1 Compile-first order

| # | File | Rationale |
|---:|---|---|
| 1 | `../kr32/examples/hello.c` | Smallest KR32 codegen/link/exit gate. |
| 2 | `../kr16/examples/hello.c` | Smallest KR16 gate. |
| 3 | `../kr16/examples/sum.c` | Function call, loop, return code on 16-bit. |
| 4 | `../kr32/examples/mainentry.c` | `argc`/`argv` ABI and varargs `printf`. |
| 5 | `../kr32/examples/hello.cpp` | First C++ gate: class + member function. |
| 6 | `../kr32/examples/types.c` | Scalar types, promotions, `printf` formats. |
| 7 | `../kr32/examples/floats.c` | FP arithmetic, compares, casts, FP ABI. |
| 8 | `../kr32/examples/varargs.c` | `va_list` over 8-byte slots, int/double mixes. |
| 9 | `../kr32/examples/strings.c` | `str*`/`mem*`, heap, `strtok`. |
| 10 | `../kr32/examples/libc-test.c` | Struct layout, `_Static_assert`, `offsetof`, heap, FP formats. |
| 11 | `../kr32/examples/binarytree.c` | Recursion, pointers, `malloc`/`free`, `strdup`; real program. |
| 12 | `../kr16/tests/integration/heap.c` | Allocator contract on 16-bit. |
| 13 | `../kr32/examples/balanced-tree.cpp` | Abstract base, virtual + dtor, inheritance, `new`/`delete`; C++ flagship. |
| 14 | `../kr32/src/cc/libc.c` | 3,058 non-blank lines (3,239 total); C self-test. |
| 15 | `../kr32/kr32os/kernel/*.c` | Freestanding C, MMIO, pointer/int casts, page tables. |
| 16 | `../kr32/examples/matrix.cpp` | Matrix/transform builtins (v1.1). |
| 17 | `../kr16/tests/kevin/C-for-U.c` | 1,473-LOC stress test. |
| 18 | `../kr16/resources/fileio.c`, `../kr16/tests/kevin/sysfile.c` | `__host_call` SYS ABI on KR16. |

### B.2 Extracted fixtures

Extract from:

- `../kr32/tests/unit/test_cc_*.cpp` — C11 features, bit-fields, `_Generic`, `typeof`, `_Alignas`, `_Atomic`, attributes.
- `../kr32/tests/cxx/test_cxx_codegen_*.cpp` — classes, inheritance, virtual, RTTI, exceptions, templates, partial specialization, lambdas, `constexpr`, `initializer_list`, range-for.
- `../kr16/src/cc/exp/tests/` — C++11/17/23 smoke tests.

Each fixture gets a feature tag, source file:line provenance, and expected output/exit code where available.

---

## Appendix C — Target profile summary

| Property | KR32 | KR16 | KR64 |
|---|---|---|---|
| Family | register | register | register |
| Pointer width | 32 | 16 | 64 |
| Data model | ILP32 | IP16 profile | LP64 |
| Endianness | little | little | little |
| `char` | signed | unsigned | TBD |
| `long` | 32 | 16 (profile) | 64 |
| `long long` | 64 | none | 64 |
| `double` | 64 | none | 64 |
| Registers | R0–R15, F0–F15, T0–T15 | R0–R4, SP/SR/PC | TBD |
| FPU | yes | no | TBD |
| Matrix | yes | no | TBD |
| Atomics | LL/SC | no | TBD |
| Threads | cluster | no | TBD |
| EH/RTTI | v1.1 | no | TBD |
| TLS | static v1.0; dynamic v1.1 | no | TBD |
| OS | kr32os | BIOS SYS | TBD |
| Debug | yes | C | TBD |
| C++ | yes | no (v1) | TBD |

---

## Appendix D — Feature matrix

### D.1 C

| Feature | KR32 | KR16 |
|---|---|---|
| C89/C99 base | yes | yes |
| `long long` | yes | no |
| `float`/`double` | yes | no |
| Designated initializers | yes | yes |
| Compound literals | yes | yes |
| `_Static_assert` | yes | yes |
| `_Alignas`/`_Alignof` | yes | yes |
| `_Generic` | yes | yes |
| `_Noreturn` | yes | yes |
| Anonymous structs/unions | yes | yes |
| `restrict` | yes (ignored) | yes (ignored) |
| `inline` | yes | yes |
| Bit-fields | yes | yes |
| `#pragma once` | yes | yes |
| VLAs | no | no |
| `_Complex` | no | no |
| `_Atomic`/threads | no | no |
| K&R definitions | no | no |
| Implicit declarations | no | no |
| `void main` | no | no |
| `typeof` | no (later) | no (later) |
| `packed` | no (later) | no (later) |

### D.2 C++

| Feature | v1.0 | v1.1 | v1.2 | Later |
|---|---|---|---|---|
| Classes, single inheritance, virtual | yes | | | |
| Multiple/virtual inheritance | | | yes | |
| Constructors/destructors, `new`/`delete` | yes | | | |
| References, overloading, namespaces | yes | | | |
| Function/class templates, partial specialization | yes | | | |
| Variadic templates | | yes | | |
| Lambdas with captures | yes | | | |
| Move/rvalue references | yes | | | |
| `auto`/`decltype`/range-for/`nullptr`/`enum class` | yes | | | |
| C++11 `constexpr` | yes | | | |
| C++14 relaxed `constexpr` | yes | | | |
| `initializer_list`/brace init | yes | | | |
| `noexcept`/`alignas`/`static_assert` | yes | | | |
| `thread_local` | static | dynamic | | |
| `if constexpr`/structured bindings/inline variables | yes | | | |
| Fold expressions (with variadic templates) | | yes | | |
| Exceptions/unwinding | | yes | | |
| RTTI/`dynamic_cast`/`typeid` | | yes | | |
| `std::unique_ptr`/`shared_ptr`/`move`/`forward` | | yes | | |
| `std::string`/`vector`/`algorithm` | | | yes | |
| C++20 `<=>`/designated init/constexpr enhancements | | | yes | |
| Concepts/`requires` | | | | after SFINAE/two-phase |
| SFINAE/two-phase/ADL | | | | post-v1 |
| `iostream`/`stringstream` | | | | v1.3 |
| `map`/`unordered_map`/`deque`/`list` | | | | v1.3 |
| C++20 library/C++23 | | | | later |
| Modules/coroutines/ranges | | | | later |

### D.3 Target extensions

Appendix D.1 and D.2 track language features; target-specific surfaces are listed here and specified in §6.4.

| Feature | KR32 | KR16 | Notes |
|---|---|---|---|
| Matrix/transform builtins | yes (v1.1) | no | §6.4; `kr32/matrix.hpp` wrappers |
| Atomics/LL-SC/fence builtins | yes | no | Runtime and guest threads |
| Host-service/syscall builtins | yes | yes | Common host-service ABI; KR16 `SYS` |
| Volatile/MMIO access helpers | yes | yes | Exact-width, non-folded access |
| Interrupt/naked/noreturn/custom conventions | yes | yes | §6.4; `docs/attributes.md` |

---

## Appendix E — Glossary

- **ABI:** Application Binary Interface; data layout, calling conventions, mangling, object/exception conventions.
- **Backend:** The target-specific part of the compiler that lowers IR to instructions, allocates registers, lays out frames, and emits assembly/objects.
- **CFA:** Canonical Frame Address; the stack address used to unwind a frame.
- **COMDAT/vague linkage:** Linker-merged duplicate definitions of inline functions, templates, vtables, and typeinfo.
- **Dialect mode:** C vs C++ language semantics selected by `-std=`.
- **IR:** Intermediate representation; the typed SSA form between Sema and the backend.
- **KDBG:** The KRCompiler symbol/debug container described in §13.
- **KRO:** The KRCompiler object container described in §11.
- **Profile:** A target’s documented data model and capabilities; the KR16 IP16 profile is intentionally non-conforming.
- **Sema:** Semantic analysis; name lookup, types, conversions, overloads, templates, `constexpr`, access control.
- **Sysroot:** The compiler’s own header/library tree for a target; replaces host headers.
- **Tier:** A corpus grouping (0 smoke, 1 C core, 2 C++ core, 3 runtime/OS, 4 stress).

---

*End of plan.md (approved v0.1, 2026-11-09).*

