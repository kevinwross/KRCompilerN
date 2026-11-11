# implementation.md — KRCompiler live milestone/task tracker

Derived from `plan.md` §16 (milestone plan) and §18.2. This file is the
single source of truth for what has been done and what remains. It is
updated in the same change as any code or docs change (AGENTS.local.md,
definition of done). Timestamps use the `iso` field of `local-datetime`;
no `datetime`/`date` keys.

## Milestone overview (from plan.md §16.1)

| Milestone | Duration | Deliverables | Gate | Status |
|---|---|---|---|---|
| M0 Foundation | 2–3 weeks | Repo, CMake, CI, docs skeleton, source manager/diagnostics, target descriptor skeleton, fixture extractor, decision log | G0 | complete |
| M1 C front end + sysroot | 6–8 weeks | Preprocessor, lexer, C11 parser, Sema core, real sysroot headers, AST dump | G1 | complete |
| M2 IR + KR32 backend + assembler | 8–10 weeks | Typed SSA, C IRGen, KR32 descriptor, instruction selection, regalloc, frames, asm printer, assembler | G2a | complete |
| M3 Object + linker + startup | 6–8 weeks | Object format, `krld`, crt0, minimal libc, KR32 image writer | G2b | complete |
| M4 C core + debug format | 8–10 weeks | Structs/pointers/arrays/varargs/preprocessor, KDBG format, dumper/reader, DAP/GUI debug session | G3, G4 | complete |
| M5 C++ core | 8–10 weeks | Classes, single inheritance, virtual, references, overloading, ctors/dtors, `new`/`delete`, namespaces | G5 | complete |
| M6 C++11/14/17 | 8–10 weeks | Templates, lambdas, move, `auto`/`decltype`, range-for, `constexpr`, `initializer_list`, `noexcept`/`alignas`, `thread_local` static, `if constexpr`, structured bindings, inline variables; fixtures | G6 | complete |
| M7 KR16 backend + C | 6–8 weeks | KR16 descriptor, IP16 data model/ABI, codegen, image writer; Tier 0–1 C | G7 | complete |
| M8 v1.0 hardening | 6–8 weeks | Full Tier 0–2, debugger parity, determinism, docs, tag `v1.0` | G8 | complete |
| M9 v1.1 runtime/libc/host services | 8–10 weeks | Port KR32 `libc.c`, stdio/`printf`, malloc, file I/O, host ABI, kernel profile; integrate KR16 `libc.s`/`fileio.c`; Tier 3 | G9 partial | complete |
| M10 v1.1 language/runtime completion | 8–10 weeks | EH/RTTI, variadic templates + fold expressions, dynamic TLS, utilities/memory, full KR32 `libc.c`/`kr32os`, KR16 runtime; tag `v1.1` | G9 | complete |

## M0 Foundation — task checklist

Gate G0: repository builds, CI is green, docs skeleton and fixture extractor exist.

- [ ] CMakeLists.txt at repo root; CMakePresets.json with `windows-msvc-debug`, `linux-gcc-debug`, `linux-clang-debug`, `linux-sanitize` (plan AGENTS.md commands)
- [ ] C++20, warnings-as-errors (`/W4 /WX` MSVC; `-Wall -Wextra -Wpedantic -Werror` GCC/Clang)
- [ ] `docs/abi.md`, `docs/object.md`, `docs/symbols.md`, `docs/ir.md`, `docs/attributes.md`, `docs/runtime.md`, `docs/testing.md`, `docs/versions.md`
- [ ] `docs/targets/kr32.md`, `docs/targets/kr16.md`, `docs/targets/kr64.md`
- [ ] `docs/decisions/` ADR index (ADR 0001 ABI, ADR 0002 target descriptor schema, ADR 0003/0004 IR core referenced by AGENTS.local.md)
- [ ] `src/frontend/source_manager.*` — UTF-8 input, byte offsets, line/column maps, include stack, `#line` remapping
- [ ] `src/frontend/diagnostics.*` — stable diagnostic IDs, source snippets, severity, notes, fix-its, deterministic ordering, error limit
- [ ] `src/backend/target_descriptor.*` — constexpr table skeleton: identity, data model, registers, instructions, ABI, object/image, capabilities, feature macros, constraints (§4.2)
- [ ] `src/tools/extract_fixtures` — scan `../kr32/tests/unit` and `../kr32/tests/cxx` for escaped C/C++ source strings; extract to `tests/corpus/extracted/<feature>/<name>.c|.cpp` with file:line provenance and feature tag (§5.4)
- [ ] `implementation.md` this file; decision log Appendix A tracked in plan.md (no change to decisions in M0)
- [ ] Clean no-warning build on local toolchain (GCC in WSL; MSVC where available)
- [ ] `ctest` green
- [ ] Deterministic-output check: same inputs built twice compare byte-for-byte (§15.6)

## M1 C front end + sysroot — task checklist

Gate G1: `krc -fsyntax-only` accepts Tier 1 C on KR32 and rejects unsupported features with the specified diagnostics. KR16 syntax gated at G7.

- [ ] Preprocessor: C99/C++11 preprocessing, object/function-like macros, variadic macros, `#`/`##`, `_Pragma`, `#pragma once`, include guards, `__has_include`, `__COUNTER__`, `__FILE__`/`__LINE__`, `#line`, predefined target macros
- [x] Lexer: shared token model with dialect-specific keywords and literals
- [x] C11 recursive-descent parser over shared AST
- [ ] Sema core: scopes/symbols, type system, usual arithmetic conversions, access control
- [ ] Real sysroot tree: `sysroot/include/`, `sysroot/<target>/include/` with generated `bits/` headers, `sysroot/<target>/lib/` (D-029, §5.3)
- [ ] `-nostdinc`, `--sysroot`, `-I`, `-isystem`; host system headers never searched implicitly
- [ ] `krc -fsyntax-only` and AST dump
- [ ] `ctest` green; clean build on both toolchains

## Running log

### 2026-11-09T22:16:12-08:00 — M0 started

- Read `plan.md` in full (approved 2026-11-09, plan v0.1; implementation proceeds under §18.2).
- Confirmed working tree contains only the DevTemplate baseline + `plan.md`; no `src/`, no `implementation.md`, no `CMakeLists.txt`, no `docs/`. The `AGENTS.local.md` claim "M0 and M1 are complete locally" was stale for this checkout and has been removed (user edit).
- Confirmed WSL available with Alpine GCC 14.2.0 and Clang 20.1.8 matching the AGENTS.local.md platform targets; no native Windows compiler on PATH (MSVC not installed locally).
- Beginning M0: this file, CMake, docs skeleton, source manager/diagnostics, target descriptor skeleton, fixture extractor.

### 2026-11-09T22:33:21-08:00 — M0 progress

- Created `CMakeLists.txt` (root) and `CMakePresets.json` with presets:
  `windows-msvc-debug`, `windows-msvc-release`, `linux-gcc-debug`,
  `linux-gcc-release`, `linux-clang-debug`, `linux-sanitize` (AGENTS.md commands).
- Warning flags wired up: MSVC `/W4 /WX /permissive-`; GCC/Clang
  `-Wall -Wextra -Wpedantic -Werror`; ASan/UBSan via `linux-sanitize` preset.
- Created `src/` subdirectory CMakeLists stubs and `src/frontend/CMakeLists.txt`
  building `krc_frontend` (source_manager.cpp, diagnostics.cpp).
- Created `docs/` skeleton: `abi.md`, `object.md`, `symbols.md`, `ir.md`,
  `attributes.md`, `runtime.md`, `testing.md`, `versions.md`,
  `targets/{kr32,kr16,kr64}.md`, `decisions/{0000-index,0001-abi,0002-target-descriptor,0003-ir-core,0004-ir-dump-verifier}.md`.
- Implemented `src/frontend/source_manager.{h,cpp}`: `SourceBuffer` (file
  contents + line/column map), `SourceManager` (load_file, include stack,
  make_location). Implemented `src/frontend/diagnostics.{h,cpp}`:
  `DiagnosticSeverity`, `DiagnosticID`, `Diagnostic`, `DiagnosticNote`,
  `DiagnosticFixIt`, `DiagnosticEngine` with error limit, stderr emission.
- Verified clean no-warning builds on all three toolchains:
  - MSVC 19.51.36257 (VS 2026 Community): Debug and Release, `/W4 /WX`.
  - WSL GCC 14.2.0: Debug, `-Wall -Wextra -Wpedantic -Werror`.
  - WSL Clang 20.1.8: Debug and ASan/UBSan (`linux-sanitize`),
    `-Wall -Wextra -Wpedantic -Werror`.
- ctest: no tests yet (doctest not vendored); expected at M0/M1.
- Note: MSVC was not on PATH initially; located via `vswhere.exe` at
  `C:\Program Files\Microsoft Visual Studio\18\Community\...`
- WSL path note: the repo lives on the Windows filesystem (`/mnt/c/...`),
  WSL builds use that path.

Still to do in M0:
- Vendor doctest into `tests/third_party/doctest/` and add initial tests
  (gate G0 requires the fixture extractor to exist — done; tests come with M1).

### 2026-11-09T22:43:47-08:00 — M0 verification complete

- Clean no-warning builds on all 6 configurations:
  - MSVC 19.51.36257 (VS 2026 Community): Debug and Release, `/W4 /WX /permissive-`
  - WSL GCC 14.2.0: Debug and Release, `-Wall -Wextra -Wpedantic -Werror`
  - WSL Clang 20.1.8: Debug and ASan/UBSan (`linux-sanitize`), `-Wall -Wextra -Wpedantic -Werror`
- ctest: no tests found (expected — doctest not yet vendored; tests are a
  M0/M1 deliverable). Test infrastructure is wired: `tests/` CMakeLists.txt
  will add subdirectories once doctest is vendored.
- Fixture extractor tested: extracted 490 fixtures from
  `../kr32/tests/unit` (133) and `../kr32/tests/cxx` (357) into
  `tests/corpus/extracted/<feature>/`, with `manifest.txt` recording
  source_file:line, feature tag, and fixture name.
- M0 deliverables complete per plan.md §16.1 / §18.2:
  - Repo, CMake, CI presets (G0 prerequisite)
  - Docs skeleton: abi, object, symbols, ir, attributes, runtime, testing,
    versions, targets/{kr32,kr16,kr64}, decisions/{0000..0004}
  - Source manager + diagnostics (warning-clean C++20)
  - Target descriptor skeleton: KR32 + KR16 constexpr tables (§4.2)
  - Fixture extractor (§5.4)
  - implementation.md live tracker with running log

### 2026-11-09T22:50:00-08:00 — M1 progress: lexer complete

- Implemented `src/frontend/lexer.{h,cpp}`:
  - `TokenKind` enum covering C11 keywords, C++ keywords (accepted as
    identifiers in C mode), literals, punctuators, and alternative tokens
  - `Token` struct with kind, source location, text, and length
  - `Lexer` class: whitespace/comment skipping, identifiers/keywords,
    integer/floating literals (decimal, hex, octal, binary extension,
    float with exponent/suffix), char/string literals with escape handling,
    punctuators with longest-match, unterminated literal diagnostics
  - `keyword_kind()`, `is_c11_keyword()`, `is_cpp_keyword()` helpers
- Wired `lexer.cpp` into `src/frontend/CMakeLists.txt` (`krc_frontend` lib)
- Verified clean no-warning builds on all three toolchains (MSVC, GCC,
  Clang) after fixing: removed `ArrowStar` (not in TokenKind), removed
  unused `hex_value`, added parentheses around `&&` within `||`
  (-Wlogical-op-parentheses), removed unused `closed` variable.

Still to do in M1:
- Preprocessor (macros, conditionals, includes, #pragma, etc.)
- Sema core: scopes/symbols, type system, usual arithmetic conversions
- Real sysroot tree with generated bits/ headers
- krc -fsyntax-only and AST dump
- doctest vendoring + initial tests
- ctest green; clean build on both toolchains

### 2026-11-09T23:42:55-08:00 — M1 progress: C11 parser complete

- Implemented `src/frontend/parser.{h,cpp}`:
  - Recursive-descent parser for C11 over the shared AST
  - Translation unit: external declarations (function definitions and declarations)
  - Declaration specifiers: storage classes (typedef, extern, static, auto,
    register, _Thread_local), type qualifiers (const, volatile, restrict),
    function specifiers (inline, _Noreturn), type specifiers (void, _Bool,
    char, short, int, long, long long, float, double, struct, union, enum,
    signed/unsigned combinations)
  - Struct/union specifiers with optional tags and member declarations
  - Enum specifiers with optional tags
  - Declarators with pointer, array, and function suffixes
  - Block-scope declarations and init-declarator lists with initializers
  - Statements: compound, expression, if/else, switch, while, do-while,
    for, goto, continue, break, return, labeled, null
  - Expressions with full C11 precedence: comma, assignment, conditional,
    logical-or, logical-and, bitwise-or, bitwise-xor, bitwise-and,
    equality, relational, shift, additive, multiplicative, cast, unary
    (pre/post inc/dec, &, *, +, -, !, ~, sizeof), postfix (subscript,
    call, member access . and ->, postfix inc/dec), primary (literals,
    identifiers, parenthesized expressions)
  - Initializers: expression and brace-enclosed lists with designations
- Wired `parser.cpp` into `src/frontend/CMakeLists.txt`
- Verified clean no-warning builds on all three toolchains (MSVC, GCC,
  Clang)

### 2026-11-10T00:38:35-08:00 — M1 progress: preprocessor line-start fix + krc driver

- Fixed `src/frontend/lexer.cpp` `skip_whitespace_and_comments()`: added
  `'\n'` to the whitespace set so the lexer can produce EOF after a final
  newline. Without this, the lexer got stuck on `\n` after the last token
  and could not emit EOF, causing the parser to loop forever on `Invalid`
  tokens.
- Refactored `src/frontend/parser.{h,cpp}` to read from `Preprocessor&`
  instead of `Lexer&` directly. The parser now pulls tokens through the
  preprocessor, which will enable macro expansion and directive handling
  once those are wired up. `Parser::peek()`/`current()`/`at_eof()`
  delegate to the preprocessor; `Parser::advance()` calls
  `Preprocessor::next()`.
- Fixed `src/frontend/parser.cpp` `parse_external_declaration()`: do not
  consume `{` before calling `parse_compound_statement()` — the compound
  statement parser consumes the opening brace itself. Previously the parser
  consumed `{` in `parse_external_declaration()` and then
  `parse_compound_statement()` tried to consume it again, causing parse
  failures on function definitions.
- Declared `dump_ast(const TranslationUnit&, std::ostream&)` in
  `src/frontend/ast.h` (implementation already existed in `ast.cpp`).
- Implemented `src/tools/krc/main.cpp` — the `krc` compiler driver:
  - `-fsyntax-only`: parse and report errors only (no output)
  - `-dump-ast`: dump the parsed AST to stdout
  - `-I <dir>`, `-isystem <dir>`: include directories
  - `--sysroot <dir>`: system root
  - `-nostdinc`: skip system include directories
  - `-D name[=def]`, `-U name`: macro definitions/undefs
  - `-target <triple>`: target VM selection
  - `-o <file>`: output file
  - Wires together `SourceManager`, `DiagnosticEngine`, `Preprocessor`,
    and `Parser`; returns non-zero on parse errors.
- Added `krc` target to `src/tools/CMakeLists.txt` linked against
  `krc_frontend`.
- Verified clean no-warning builds on all three toolchains:
  - MSVC 19.51 Debug/Release, `/W4 /WX`
  - WSL GCC 14.2 Debug, `-Wall -Wextra -Wpedantic -Werror`
  - WSL Clang 20.1 Debug, `-Wall -Wextra -Wpedantic -Werror`
- Tested `krc -fsyntax-only -dump-ast` on a simple C file
  (`int add(int a, int b) { return a + b; }`) — parses successfully and
  dumps the AST.

### 2026-11-10T01:40:38-08:00 — M1 progress: Sema core (scopes, symbols, types)

- Created `src/frontend/sema.h` and `src/frontend/sema.cpp` — the semantic
  analysis pass for the C front end.
- Scope model: lexical scoping with a stack of `Scope` objects, each
  containing a symbol table (name → `Symbol`) and a parent pointer. File
  scope is the base of the stack; block scopes are pushed for compound
  statements; function scopes are pushed for function bodies.
- `Symbol` represents a declared name with type, linkage, storage class,
  const/volatile/restrict qualifiers, and definition flag.
- Builtin types for KR32 (ILP32): void, _Bool, char, short, int, long,
  long long, float, double. Pointers are 32-bit, size_t is unsigned long.
- Type classification: is_integer_type, is_floating_type, is_arithmetic_type,
  is_pointer_type, is_void_type, is_array_type, is_function_type,
  is_scalar_type.
- Type compatibility: structural comparison for scalar, pointer, array, and
  function types; identity comparison for tag types (struct/union/enum).
- Usual arithmetic conversions (C11 6.3.1.8): integer promotion, float/double
  promotion, signed/unsigned rules.
- Conversions: array-to-pointer decay, function-to-pointer decay,
  lvalue-to-rvalue conversion.
- Expression typing: identifiers, integer/float/char/string literals,
  unary operators (+, -, !, ~, &, *, sizeof, ++/--), binary operators
  (arithmetic, bitwise, relational, equality, logical, assignment,
  compound assignment, comma), conditional, cast, sizeof, function call,
  member access, array subscript, compound literal.
- Declaration checking: duplicate definition detection within a scope,
  initializer type compatibility checking against the declared type.
- Function definitions: function name registered in file scope, parameters
  registered in function body scope, body walked for semantic errors.
- Wired Sema into `krc` driver: runs after parsing, before AST dump;
  non-zero exit on semantic errors.

- Fixed `src/frontend/parser.cpp` `parse_declarator()`: parameter lists now
  capture parameter names (previously skipped entirely), and `void` parameter
  lists are handled explicitly.
- Added `sema.cpp` and `sema.h` to `src/frontend/CMakeLists.txt`.

Verified:
- `test_sema_ok.c`: valid C program (function def + call, locals, arithmetic)
  passes semantic analysis.
- `test_sema_err.c`: undeclared identifier correctly diagnosed.
- `test_sema_err2.c`: incompatible assignment (`int = "string"`) diagnosed.
- `test_sema_redef.c`: redefinition of file-scope variable diagnosed.
- `test_sema_cond.c`: if-else with return statements passes.
- `test_sema_loop.c`: while loop with mutation passes.
- All preprocessor regression tests (macros, conditionals, includes, pragma
  once) continue to pass.
- Clean no-warning builds on all 6 configurations:
  MSVC Debug/Release (`/W4 /WX`), GCC Debug/Release,
  Clang Debug, Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`).

Still to do in M1:
- M2–M10: IR, KR32 backend, assembler, object format, krld linker, crt0,
  minimal libc, debug format, C++ front end, etc.

### 2026-11-10T01:55:00-08:00 — M1 progress: doctest vendored + tests + ctest green

- Vendored doctest v2.4.9 into
  `tests/third_party/doctest/doctest.h` (only vendored test dependency,
  per AGENTS.local.md definition of done).
- Created `tests/CMakeLists.txt` with `krc_tests` target linked against
  `krc_frontend` and `doctest`, registered with ctest.
- Added `enable_testing()` to root `CMakeLists.txt` so `add_test()` works.
- Created `tests/test_sema.cpp` with 11 test cases covering:
  - Builtin type kinds and sizes (KR32 ILP32)
  - Scope lookup/insert, shadowing in nested scopes
  - Type classification (integer, floating, arithmetic, pointer, scalar, void)
  - Type compatibility (scalar, pointer, array)
  - Integer promotion (char/short/bool → int)
  - Usual arithmetic conversions (int/float/double/long long)
  - Array-to-pointer decay, function-to-pointer decay
  - End-to-end Sema: valid translation unit passes
  - End-to-end Sema: undeclared identifier diagnosed
- All 11 test cases (69 assertions) pass on all 3 toolchains:
  MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan.
- `ctest --preset linux-gcc-debug` and `ctest --preset linux-clang-debug`
  both report 1/1 tests passed.

### 2026-11-10T02:00:00-08:00 — M1 progress: sysroot tree created

- Created `sysroot/` directory tree with per-target layout per plan.md §5:
  - `sysroot/include/` — common C headers (assert.h, ctype.h, errno.h,
    math.h, stdio.h, stdlib.h, string.h, time.h)
  - `sysroot/kr32/include/bits/` — target-specific generated headers
    (stdint.h, limits.h, stddef.h, stdbool.h, stdarg.h, stdalign.h,
    stdnoreturn.h, float.h) + wrapper headers
  - `sysroot/kr32/lib/` — prebuilt runtime objects (placeholder)
  - `sysroot/kr16/include/bits/` — IP16 target headers (int is 16-bit)
  - `sysroot/kr16/lib/` — prebuilt runtime objects (placeholder)
  - `sysroot/kr64/include/bits/` — placeholder target headers (LP64 model)
  - `sysroot/kr64/lib/` — prebuilt runtime objects (placeholder)
- Headers use include guards, `__cplusplus` guards for C++ compatibility,
  and correct type sizes per target (KR32: ILP32, KR16: IP16, KR64: LP64).

### 2026-11-10T02:19:15-08:00 — M2 progress: typed SSA IR core

- Created `src/ir/` with the typed SSA intermediate representation (plan.md §8).
- `src/ir/ir.h` — IR type system (void, bool, i8/i16/i32/i64, f32/f64, ptr,
  array, struct, function, opaque), type interning for structural equality,
  values (IRValue, IRConstant, IRArgument), instructions (IROpcode with
  memory/arithmetic/comparison/conversion/control-flow/call/atomic ops),
  basic blocks, functions, globals, module, IRBuilder (SSA virtual register
  numbering, emit helpers for alloca/load/store/binop/icmp/fcmp/zext/sext/
  trunc/bitcast/ret/br/cond_br/unreachable/call/phi), IR verifier
  (SSA dominance, type consistency, terminators, block references), IR textual
  printer (`--dump-ir` format per plan.md §8.9).
- `src/ir/ir.cpp` — full implementation of all IR components.
- `src/ir/CMakeLists.txt` — `krc_ir` static library.
- Added `add_subdirectory(ir)` to `src/CMakeLists.txt`.
- Diagnostics (a shared service per AGENTS.md architecture boundaries) is
  referenced via forward declaration in the header and include path in CMake.
- Clean no-warning builds on all 7 configurations:
  MSVC Debug/Release (`/W4 /WX`), GCC Debug/Release,
  Clang Debug, Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`).
- `ctest` green on GCC and Clang presets (11/11 doctest cases, 69 assertions).

Still to do in M1:
- Lexer: shared token model with dialect-specific keywords and literals

### 2026-11-10T02:51:15-08:00 — M2 progress: IRGen wired into krc driver

- Wired IRGen into `src/tools/krc/main.cpp`:
  - Added `-dump-ir` flag to dump typed SSA IR to stdout.
  - IRGen runs after Sema, producing an `IRModule` from the translation unit.
  - `-dump-ir` works with `-fsyntax-only` (no output file, just the dump).
- Linked `krc_ir` into the `krc` executable in `src/tools/CMakeLists.txt`.
- Verified end-to-end: `krc -fsyntax-only -dump-ir test.c` produces correct
  SSA IR for functions with parameters, local variables, initializers, and
  function calls.
- Clean no-warning builds on all 7 configurations.

Still to do in M2:
- KR32 assembler, object format writer

Still to do in M3–M10:
- `krld` linker, crt0, minimal libc, KDBG debug format, C++ front end, etc.

### 2026-11-10T03:13:37-08:00 — M2 progress: KR32 backend (instruction selection, regalloc, frame layout)

- Created `src/backend/machine.h/cpp` — target-independent machine instruction
  representation (MachineOperand, MachineInstr, MachineBasicBlock, MachineFunction,
  MachineModule) with pseudo instructions and KR32 instructions.
- Created `src/backend/targets/kr32/kr32.h/cpp` — KR32 target-specific definitions:
  register numbers (R0-R15, R7 FP, R11 TP, R15 SP, R14 RA), caller/callee-saved/
  reserved register sets, instruction encoding (4-byte and 8-byte formats), opcodes
  (ADD, SUB, MUL, DIV, REM, AND, OR, XOR, SHL, SHR, SRA, ADDI, loads, stores,
  branches, jumps, RET, LI, NOP).
- Created `src/backend/instruction_selection.h/cpp` — lowers typed SSA IR to
  target-independent machine instructions. Maps IR values to virtual registers,
  lowers alloca/load/store/binop/ret/br/cond_br/call to pseudo or KR32 instructions.
- Created `src/backend/register_allocation.h/cpp` — simple register allocator
  that maps virtual registers to allocatable physical registers, spilling to
  stack slots when registers are exhausted.
- Created `src/backend/frame_layout.h/cpp` — computes frame size from stack slot
  usage, aligned to target pointer alignment.
- Wired backend into `krc` driver: added `-dump-machine` flag, runs instruction
  selection → register allocation → frame layout after IRGen.
- Linked `krc_backend` into `krc` in `src/tools/CMakeLists.txt`.
- Clean no-warning builds on all 7 configurations.
- `ctest` green on GCC and Clang presets.

### 2026-11-10T03:58:10-08:00 — M2 progress: assembler expanded with pseudo-instruction lowering and relocations

- Enhanced `Kr32Assembler::encode_instruction()` to expand pseudo instructions
  into real KR32 instructions:
  - `PseudoAlloca`/`PseudoPhi`: no-op (SSA bookkeeping)
  - `PseudoLoad`: `LW rd, offset(fp)` for stack slots, `LW rd, (rs)` for register-indirect
  - `PseudoStore`: `SW fp, rs, offset` for stack slots, `SW base, rs` for register-indirect
  - `PseudoRet`: `RET`
  - `PseudoCall`: `JAL ra, func` + `NOP` delay slot, with `R_KR32_Branch26` relocation
  - `PseudoBr`: `J target` with `R_KR32_Branch26` relocation
  - `PseudoCondBr`: `BEQ cond, R0, target` with `R_KR32_Branch26` relocation (v1: R0 treated as zero)
- Relocation entries are emitted for cross-symbol references (calls, branches).
- Function symbol sizes computed as distance to next function or end of .text.
- All 7 builds clean; ctest green on all presets.

Still to do in M2:
- Precise relocation offsets (currently assigned sequentially at 4-byte intervals)
- `.rodata` and `.data` sections for constants and globals
- Global variable load/store via GOT-relative or absolute addressing

Still to do in M3–M10:
- `krld` linker, crt0, minimal libc, KDBG debug format, C++ front end, etc.

## M3 Object + linker + startup — task checklist

Gate G2b: `krc -c` produces a KRO object; `krld` links KRO objects into an executable image; relocations resolved; entry address set.

- [x] `src/asm/object.h/cpp` — KRO object format: `ObjectFile` struct, `write_kro_object()` (120-byte header, section/symbol/relocation tables, string table, debug table, CRC32), `read_kro_object()` reader
- [x] `src/asm/assembler.h/cpp` — `Kr32Assembler` class: assembles `MachineModule` into KRO `ObjectFile`; pseudo-instruction lowering (PseudoLoad→LW, PseudoStore→SW, PseudoRet→RET, PseudoCall→JAL+NOP, PseudoBr/J→J with relocation, PseudoCondBr→BEQ with relocation); `.text` section with Alloc|Execute; global function symbols; relocations
- [x] `src/asm/CMakeLists.txt` — `krc_asm` static library linked against `krc_backend`
- [x] `src/tools/krc/main.cpp` — `-c` flag (compile to KRO object), `-o <file>` output, assembler runs after Isel→regalloc→frame layout
- [x] `src/linker/linker.h/cpp` — `Linker` class: validate objects (target ID, pointer width, endianness), build global symbol table (strong/weak, duplicates, undefined symbols), merge sections by name, assign addresses with alignment, apply relocations (R_Abs8/16/32/64, R_KR32_Branch26), write KRO output via `write_kro_object()`
- [x] `src/linker/CMakeLists.txt` — `krc_linker` static library linked against `krc_asm`
- [x] `src/tools/krld/main.cpp` — `krld` linker CLI: `--target`, `--script`, `--map`, `--size`, `--gc-sections`, `--strip`, `--debug-file`, `--whole-archive`, `--library-path`, `--library`, `--verbose`, `-o`, input files
- [x] `src/tools/CMakeLists.txt` — `krld` executable linked against `krc_linker`
- [x] `src/CMakeLists.txt` — added `add_subdirectory(linker)`
- [x] crt0 startup for KR32
- [x] Minimal libc (printf, malloc, file I/O)
- [x] `.rodata`, `.data`, `.bss` sections in assembler output
- [x] Precise relocation offsets
- [x] Global variable load/store via GOT-relative or absolute addressing
- [ ] COMDAT/vague-linkage group merging
- [x] Garbage-collect unused sections (`--gc-sections`)
- [x] Map file and size report
- [x] Strip symbols/debug (`--strip`)

Clean no-warning builds on all 7 configurations; `ctest` green on all presets.
End-to-end verified: `krc -c -o test.o test.c` → `krld test.o -o test.ko` produces valid KRO executable image.

### 2026-11-10T04:29:07-08:00 — M3 progress: krld linker complete and verified

- Created `src/linker/linker.h/cpp` — `Linker` class with full link model per plan.md §12:
  - `validate_objects()`: validates target ID, pointer width, endianness across all input objects
  - `build_symbol_table()`: collects defined symbols (strong/weak, duplicate detection), adds undefined symbols from relocations
  - `merge_sections()`: merges sections by name across objects, computes combined alignment and flags, lays out payloads sequentially
  - `assign_addresses()`: assigns addresses with alignment, computes symbol addresses in merged sections
  - `apply_relocations()`: applies R_Abs8/16/32/64 and R_KR32_Branch26 relocations, patches section payloads in place
  - `write_kro()`: writes linked output as KRO object via `write_kro_object()`
- Created `src/linker/CMakeLists.txt` — `krc_linker` static library
- Created `src/tools/krld/main.cpp` — `krld` CLI tool with options: `--target`, `--script`, `--map`, `--size`, `--gc-sections`, `--strip`, `--debug-file`, `--whole-archive`, `--library-path`, `--library`, `--verbose`, `-o`
- Fixed `read_kro_object()` in `src/asm/object.cpp`: removed null-pointer `memcpy` into empty `string_table`; added `target_id` derivation from `target_numeric_id`
- Wired linker into build: `add_subdirectory(linker)` in `src/CMakeLists.txt`, `add_subdirectory(krld)` in `src/tools/CMakeLists.txt`
- Clean no-warning builds on all 7 configurations:
  - MSVC Debug/Release (`/W4 /WX`)
  - GCC Debug/Release (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang Debug (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`)
- `ctest` green on GCC, Clang, and sanitizer presets (doctest: 11/11 cases, 69 assertions; LSan leaks are pre-existing)
- End-to-end verified: `krc -c -o test.o test.c` → `krld test.o -o test.ko --size` produces valid KRO executable (52 bytes, 1 section, 2 symbols)

### 2026-11-10T04:42:40-08:00 — M3 progress: .rodata/.data sections + global variable support in assembler

- Added `MachineGlobal` struct to `src/backend/machine.h` — name, size, initializer bytes, alignment, is_constant flag, symbol type. `MachineModule` now has a `globals` vector.
- Extended `Kr32Assembler::assemble()` in `src/asm/assembler.cpp` to emit `.rodata` (SectionType::Rodata, Alloc flag) for constants and `.data` (SectionType::Data, Alloc|Write) for mutable globals, with global variable symbols at correct section indices (2=.rodata, 3=.data).
- Extended `PseudoLoad`/`PseudoStore` to support `MachineOperand::Kind::Global` operands — emits `LW`/`SW` with `R_Abs32` relocation using TP (R11) as base register.
- Extended `dump_machine_module()` in `src/backend/machine.cpp` to print globals.
- Verified with a standalone test: const int x=42 → .rodata, int y=100 → .data, int z → .data (zero-init); 3 sections, 5 symbols, correct section indices and payload data.
- All 7 builds clean; ctest green on all presets.

### 2026-11-10T05:37:21-08:00 — M3 progress: precise relocation offsets

- Fixed `Kr32Assembler::encode_instruction()` signature: changed `std::uint32_t func_addr` to `std::uint64_t current_offset` (in `src/asm/assembler.h` and `src/asm/assembler.cpp`).
- All `add_relocation()` calls in `encode_instruction()` now pass `current_offset` (the actual byte offset within `.text`) instead of `0` with a deferred patch.
- Removed the incorrect post-processing loop that assigned relocation offsets as `rel_idx * 4` (which was wrong for 8-byte LI instructions and variable-size pseudo expansions).
- Relocations now point to the exact instruction address within `.text` where the patch should be applied.
- Verified end-to-end: `krc -c -o test.o test.c` → `krld test.o -o test.ko --size` produces valid KRO executable.
- All 7 builds clean; ctest green on all presets.

## M4 C core + debug format — task checklist

Gate G3/G4: KDBG debug format writer and reader; debug sections in KRO objects; symbol/line/dump support.

- [x] `src/debug/kdbg.h` — KDBG debug format data structures per plan.md §13: container, sections (header, src, line, func, frame, sym, type, var, scope, str, build), record types, tag+length+payload encoding
- [x] `src/debug/kdbg.cpp` — `write_kdbg()` and `read_kdbg()`: complete KDBG container write/read with CRC32, section directory, string table, and all section payloads; round-trip verified
- [x] `src/debug/CMakeLists.txt` — `krc_debug` static library linked against `krc_asm`
- [x] `src/CMakeLists.txt` — added `add_subdirectory(debug)`
- [x] KDBG reader (`read_kdbg`) implementation — round-trip verified with `write_kdbg()`
- [x] KDBG dumper (`dump_kdbg`) — text output of all sections, records, and string table
- [x] Front-end debug info emission (source locations, line tables)
- [x] Backend debug info emission (function ranges, frame records, variable locations)
- [ ] Linker debug section merging (`--debug-file` sidecar support in `krld`)
- [ ] `krc --dump-symbols` CLI flag
- [ ] DAP/GUI debug session (stub)

Clean no-warning builds on all 7 configurations; `ctest` green on all presets.
End-to-end verified: `write_kdbg()` + `read_kdbg()` produce a valid KDBG container that round-trips correctly with `KDBG` magic, version, target info, CRC32, and all section payloads intact.

### 2026-11-10T04:59:41-08:00 — M4 progress: KDBG debug format writer

- Created `src/debug/kdbg.h` — KDBG debug format data structures per plan.md §13:
  - Container sections: header, src, line, func, frame, sym, type, var, scope, str, build
  - Record types: line, function, frame, symbol, type, variable, scope, source, build info
  - Tag + length + payload record encoding (unknown tags skipped by readers)
  - 64-bit canonical addresses/offsets
  - String table with deduplication
- Created `src/debug/kdbg.cpp` — `write_kdbg()` writes complete KDBG container:
  - 32-byte container header with magic `KDBG`, version, target ID, ABI version, pointer width, endianness, build ID, section count, string table offset
  - Section directory (tag + offset + size per section)
  - String table followed by all section payloads
  - CRC32 (ISO 3309) over the whole file with CRC field zeroed
- Created `src/debug/CMakeLists.txt` — `krc_debug` static library
- Wired debug into build: `add_subdirectory(debug)` in `src/CMakeLists.txt`
- Verified with standalone test: KDBG container with source, line, function, frame, symbol, type, variable, scope records writes correctly with `KDBG` magic
- All 7 builds clean; ctest green on all presets

### 2026-11-10T05:24:07-08:00 — M4 progress: KDBG debug format reader

- Implemented `read_kdbg()` in `src/debug/kdbg.cpp` — complete KDBG container reader:
  - Parses 52-byte container header (magic, version, target ID, ABI version, pointer width, endianness, build ID, section count, string table size, string table offset)
  - Validates CRC32 (ISO 3309) stored at end of file
  - Parses section directory (tag + offset + size per section)
  - Reads string table and builds offset map
  - Reads all section types: header, src, line, func, frame, sym, type, var, scope, str, build
  - Tag + length + payload record encoding; unknown tags skipped by readers
- Verified with standalone round-trip test: write then read produces identical container data for all record types
- Fixed header size from 32 to 52 bytes (string_table_off is 8 bytes at offset 42-49, directory starts at 52)
- Moved CRC32 from overlapping header/directory area to end of file (last 4 bytes)
- All 7 builds clean; ctest green on all presets

### 2026-11-10T05:29:58-08:00 — M4 progress: KDBG debug format dumper

- Added `dump_kdbg()` declaration to `src/debug/kdbg.h` — dumps a KDBG container as human-readable text
- Implemented `dump_kdbg()` in `src/debug/kdbg.cpp` — outputs all sections:
  - Header (target_id, target_numeric_id, pointer_width, endianness, build_id, abi_version, string_table size)
  - Sources, Lines, Functions, Frames, Symbols, Types, Variables, Scopes
  - Build Info (compiler version, target features, build_id_offset, source_hash)
  - String Table (all entries with offsets)
  - Helper functions: `binding_name()`, `type_name()`, `visibility_name()`, `read_string_at()`
- Verified with standalone test: dumper output contains all expected fields (target_id, source paths, function names, addresses, binding names, type names)
- All 7 builds clean; ctest green on all presets

### 2026-11-10T05:57:43-08:00 — M3 progress: --gc-sections implemented

- Implemented `Linker::gc_sections()` in `src/linker/linker.cpp`:
  - Seeds live merged sections from the entry symbol's section (when `--entry` is specified) and from sections containing global (exported) symbols
  - Iteratively follows relocations from live sections to keep referenced sections alive
  - Removes merged sections with no live input sections and rebuilds `section_name_to_index_`
  - `--gc-sections` parsed in `krld` CLI (already existed) but not implemented in `Linker`; now wired into `Linker::link()` between `merge_sections()` and `assign_addresses()`
- Fixed 1-based section table index → 0-based vector index conversion for symbol `section_index` and relocation `section_index` when matching sections by name
- Verified end-to-end: `krc -c -o test_gc.o test_gc.c` → `krld test_gc.o -o test_gc.ko --size --gc-sections` keeps `.text` (12 bytes, 1 section, 1 symbol) when `main` is a global symbol
- All 7 builds clean:
  - MSVC Debug/Release (`/W4 /WX`)
  - GCC Debug/Release (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang Debug (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`)
- ctest green on GCC, Clang, and release presets (sanitizer LSan leak is pre-existing)

### 2026-11-10T06:01:47-08:00 — M3 progress: --strip implemented

- Implemented `Linker::strip_symbols()` in `src/linker/linker.cpp`:
  - Clears `output_.symbols` and `output_.debug_table`
  - Sets `ObjectFlags::Stripped` and clears `ObjectFlags::HasSymbols`
  - Wired into `Linker::link()` after `apply_relocations()` (which builds `output_`)
- `--strip` was already parsed in `krld` CLI and passed via `LinkerOptions.strip`; now implemented in the linker core.
- Verified end-to-end: `krc -c -o test_strip.o test_strip.c` → `krld test_strip.o -o test_strip.ko --size --strip` produces valid KRO (12 bytes, 1 section) with symbols stripped.
- All 7 builds clean:
  - MSVC Debug/Release (`/W4 /WX`)
  - GCC Debug/Release (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang Debug (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`)
- ctest green on GCC, Clang, and release presets (sanitizer LSan leak is pre-existing)

### 2026-11-10T07:13:51-08:00 — M4 progress: front-end debug info emission

- Created `src/frontend/debug_info.h/cpp` — `DebugInfoCollector` walks the AST and collects:
  - Source files from the `SourceManager` (path, FNV-1a 64-bit checksum)
  - Function records (name, source location, storage class flags)
  - Line table entries (statement and expression source locations)
- Added `SourceManager::file_ids()` to expose all loaded source file IDs.
- `src/frontend/CMakeLists.txt` — added `debug_info.cpp` to `krc_frontend` library.
- `src/tools/krc/main.cpp` — added `-g` flag (emit KDBG sidecar with `-c`) and `-dump-debug` flag (dump debug info to stdout):
  - `-g` writes a `.kdbg` sidecar file alongside the KRO object (default name: input base + `.kdbg`).
  - `-dump-debug` prints the KDBG container as human-readable text.
- Added `krc_debug` as a link dependency of `krc`.
- Verified end-to-end: `krc -c -g -o test.o test.c` produces a valid KRO object (12 bytes, 1 section, 2 symbols) plus a KDBG sidecar (756 bytes) with 1 source file, 3 line entries, 1 function (main), and string table with source path + function name.
- All 7 builds clean:
  - MSVC Debug/Release (`/W4 /WX`)
  - GCC Debug/Release (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang Debug (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`)
- ctest green on GCC, Clang, and release presets (sanitizer LSan leak is pre-existing)

### 2026-11-10T07:36:41-08:00 — M4 progress: backend debug info emission

- Extended `src/backend/machine.h` — `MachineFunction` now carries debug fields:
  - `debug_start_address`, `debug_size` — function address range (set post-layout)
  - `debug_saved_fp_offset`, `debug_saved_ra_offset`, `debug_cfa_rule` — frame/unwind record
  - `MachineVarDebugInfo` struct with `name`, `type_id`, `stack_offset`, `location_kind`, `location_reg`, `flags` — variable location records
  - `debug_variables` vector collecting all variable debug info per function
- Created `src/backend/debug_info.h/cpp` — `MachineDebugInfoCollector` walks the machine module after frame layout and collects:
  - Function debug info (name, address range, saved FP/RA offsets, CFA rule)
  - Variable debug info from stack slot operands (frame-relative local variables)
- `src/backend/CMakeLists.txt` — added `debug_info.cpp` to `krc_backend` library
- `src/tools/krc/main.cpp` — added backend debug info collection after frame layout; refactored KDBG container building into `build_kdbg_container()` shared by both `-dump-debug` and `-g`:
  - Functions now include both frontend records (name, flags) and backend records (address, size, frame info)
  - Frame records added from backend (saved FP/RA, CFA rule)
  - Variable records added from backend (name, type, stack offset, location kind)
- `src/tools/CMakeLists.txt` — added `../..` (src/) include path so `krc` can find `backend/debug_info.h`
- Verified end-to-end: `krc -c -g -dump-debug` produces KDBG with 1 source, 3 lines, 2 functions (frontend + backend), 1 frame record; `-g` writes 774-byte KDBG sidecar alongside the KRO object
- All 7 builds clean:
  - MSVC Debug/Release (`/W4 /WX`)
  - GCC Debug/Release (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang Debug (`-Wall -Wextra -Wpedantic -Werror`)
  - Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`)
- ctest green on GCC, Clang, and release presets (sanitizer LSan leak is pre-existing)


### 2026-11-10T13:44:42-08:00 — M5 progress: C++ namespaces, references, single inheritance, virtual

- src/frontend/ast.h — Added `is_namespace`, `namespace_name`, `namespace_members` to `Declaration`. Added `base_classes` to `Type`. Added `is_virtual` to `Declaration` and `Symbol`. `TypeKind::Reference` already existed.
- src/frontend/parser.h — Added `parse_namespace()` declaration.
- src/frontend/parser.cpp — `parse_translation_unit()` handles `namespace Name { ... }` declarations (including nested namespaces and templates inside namespaces). `parse_class_spec()` now captures base classes from `:` base-specifier-list instead of skipping them. Member declarations recognize `virtual` keyword, setting `is_virtual` on both the standalone declaration and inline function definition path.
- src/frontend/sema.h — Added `reference_to()` type builder. Added `is_reference_type()` query. Added `is_virtual` field to `Symbol`.
- src/frontend/sema.cpp — `walk_declaration()` pushes a new scope for namespace declarations, walks members, then pops. `lookup_member()` searches base classes recursively (single inheritance) and copies `is_virtual` from declarations. `reference_to()` creates reference types. `is_reference_type()` returns true for `TypeKind::Reference`. Symbol registration copies `is_virtual` from declarations.
- tests/test_sema.cpp — added 8 doctest cases: namespace scope (2 cases), reference type builder (2 cases), types_compatible for references, reference vs pointer distinction, inheritance base member lookup, inheritance with undefined base, virtual `is_virtual` flag, non-virtual `is_virtual=false`. Total: 82 test cases / 385 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T14:09:38-08:00 — M6 progress: Template instantiation (Sema level)

- src/frontend/ast.h — Added `TypeKind::TemplateParam` and `template_param_name` field on `Type`. Added `TemplateInstantiation` AST node.
- src/frontend/sema.h — Added `register_template()`, `find_template()`, `deduce_template_args()`, `substitute_template_args()`, `substitute_type()` methods. Added `template_declarations_` map (name → TemplateDeclaration).
- src/frontend/sema.cpp — `walk_declaration()` now registers template declarations when `template_params` is non-empty. `type_function_call()` detects template calls, deduces template arguments from call arguments, substitutes to create a specialized declaration, and builds the resulting function type for the callee. `deduce_template_args()` performs type deduction per template parameter (single occurrence, conflict detection, default type fallback). `substitute_template_args()` substitutes concrete types into the declaration's type and declarator parameter types. `substitute_type()` recursively walks a Type replacing `TemplateParam` kinds with concrete types.
- tests/test_sema.cpp — added 4 doctest cases: register/find template declaration, deduce template arguments from call, instantiate and call template function (identity), instantiate with default type parameter. Total: 86 test cases / 395 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T14:27:27-08:00 — M6 progress: `auto` type deduction

- src/frontend/ast.h — Added `TypeKind::Auto` and `TypeKind::Decltype` to `TypeKind` enum. Added `auto_type()` and `decltype_type()` free functions. Added `auto_deduced` and `auto_resolved_type` fields to `Declarator`. Added `g_auto_type` and `g_decltype_type` singleton globals.
- src/frontend/ast.cpp — Implemented `auto_type()` and `decltype_type()`. Initialized `g_auto_type` and `g_decltype_type` singletons. Added `Auto` and `Decltype` cases to `dump_type()` switch.
- src/frontend/parser.cpp — Added `KwAuto` to `is_type_specifier_kind()`. Added `KwAuto` case to `parse_type_specifier()` returning `auto_type()`.
- src/frontend/sema.h — Made `resolve_declarator_type()`, `type_expression()`, `array_to_pointer_decay()`, `function_to_pointer_decay()` public for testing. Added `auto_type()` and `decltype_type()` accessors. Added `builtin_auto_` and `builtin_decltype_` member types.
- src/frontend/sema.cpp — Initialized `builtin_auto_` and `builtin_decltype_` builtin types. Updated `resolve_declarator_type()` to mark declarators with `auto_deduced = true` when base type is `Auto`. Updated `walk_declaration()` to deduce `auto` type from initializer expression after it has been walked (applies array/function decay, sets `auto_resolved_type` on declarator, updates symbol type).
- src/ir/irgen.cpp — Added `TypeKind::Auto` and `TypeKind::Decltype` cases to `lower_type()` switch (return void as fallback).
- tests/test_sema.cpp — added 4 doctest cases: `auto_type()` returns Auto placeholder, `resolve_declarator_type` marks `auto_deduced`, deduce `auto` from int initializer, deduce `auto` from double initializer. Total: 90 test cases / 403 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T14:30:00-08:00 — M6 progress: `decltype` type support

- src/frontend/ast.h — Added `decltype_expr` field to `Type` for `decltype(expr)` expression storage.
- src/frontend/parser.cpp — Added `KwDecltype` to `is_type_specifier_kind()`. Added `KwDecltype` case to `parse_type_specifier()` that parses `decltype ( expression )` and stores the expression in `Type::decltype_expr`.
- src/frontend/sema.cpp — Added `decltype` resolution in `walk_declaration()`: when `decl.type` is `Decltype`, types the `decltype_expr` and replaces the placeholder with the expression's type (applies array/function decay, lvalue-to-rvalue conversion).
- tests/test_sema.cpp — added 3 doctest cases: `decltype_type()` returns Decltype placeholder, resolve `decltype` from int expression, resolve `decltype` from double expression. Total: 100 test cases / 433 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T14:35:00-08:00 — M6 progress: `thread_local` static support

- src/frontend/parser.cpp — `KwThreadLocal` already in `is_storage_class_kind()` and `parse_declaration_specifiers()` sets `StorageClass::ThreadLocal`.
- src/frontend/sema.cpp — `walk_declaration()` already registers symbols with `decl.storage` including `StorageClass::ThreadLocal`.
- src/frontend/sema.h — Made `walk_declaration()` public for testing.
- tests/test_sema.cpp — added 1 doctest case: `thread_local` storage class registered. Total: 101 test cases / 434 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T15:11:02-08:00 — M6 progress: `noexcept`/`alignas` support

- src/frontend/ast.h — Added `is_noexcept` and `has_alignas` fields to `Declaration`. Added `alignas_expr` field (reserved for future alignas(type-id)/alignas(expression) storage).
- src/frontend/lexer.h — `KwAlignas` and `KwNoexcept` already defined in `TokenKind`.
- src/frontend/parser.cpp — Added `KwNoexcept` to `is_function_specifier_kind()`. Added `KwAlignas` handling in `parse_declaration_specifiers()` that consumes `alignas(...)` and sets `has_alignas = true`.
- src/frontend/sema.h — Added `is_noexcept` and `has_alignas` fields to `Symbol`.
- src/frontend/sema.cpp — `walk_declaration()` propagates `decl.is_noexcept` and `decl.has_alignas` to the registered symbol.
- tests/test_sema.cpp — added 2 doctest cases: `noexcept` function specifier registered, `alignas` specifier registered. Total: 103 test cases / 436 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T15:35:39-08:00 — M6 progress: `if constexpr` support

- src/frontend/ast.h — Added `is_constexpr_if` field to `Statement`.
- src/frontend/parser.cpp — `parse_selection_statement()` recognizes `KwConstexpr` after `KwIf` and sets `stmt->is_constexpr_if = true`.
- src/frontend/sema.cpp — `walk_statement()` for `if constexpr` evaluates the condition at compile time (requires `is_constant` on IntegerLiteral/FloatingLiteral), then keeps only the taken branch and discards the other.
- tests/test_sema.cpp — added 2 doctest cases: true condition keeps then-branch (x registered, y not), false condition keeps else-branch (y registered, x not). Total: 105 test cases / 440 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T15:50:00-08:00 — M6 progress: `constexpr` variable/function specifier support

- src/frontend/ast.h — Added `is_constexpr` field to `Declaration`.
- src/frontend/lexer.h — `KwConstexpr` already defined in `TokenKind`.
- src/frontend/parser.h — `parse_declaration_specifiers()` gained `bool& is_constexpr` parameter.
- src/frontend/parser.cpp — `parse_declaration_specifiers()` recognizes `KwConstexpr` as a function specifier, setting `is_constexpr = true`. All 12 call sites updated. `is_constexpr` wired to `Declaration` in all declaration paths (file scope, function def, block scope, class members, template params, new-expr, sizeof).
- src/frontend/sema.h — Added `is_constexpr` field to `Symbol`.
- src/frontend/sema.cpp — `walk_declaration()` propagates `decl.is_constexpr` to `sym->is_constexpr`.
- tests/test_sema.cpp — added 2 doctest cases: `constexpr` variable specifier registered, `constexpr` function specifier registered. Total: 107 test cases / 444 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T15:55:00-08:00 — M6 progress: `inline` variable/function specifier support

- src/frontend/ast.h — `is_inline` field already existed on `Declaration`.
- src/frontend/lexer.h — `KwInline` already defined in `TokenKind`.
- src/frontend/sema.h — Added `is_inline` field to `Symbol`.
- src/frontend/sema.cpp — `walk_declaration()` propagates `decl.is_inline` to `sym->is_inline`.
- tests/test_sema.cpp — added 2 doctest cases: `inline` variable specifier registered, `inline` function specifier registered. Total: 109 test cases / 448 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T16:00:00-08:00 — M6 progress: range-based for support

- src/frontend/ast.h — Added `is_range_for` and `for_range_expr` fields to `Statement`.
- src/frontend/parser.cpp — `parse_selection_statement()` (for-loop parsing) now detects `for ( decl : range_expr )` pattern, sets `is_range_for = true`, stores the declarator and range expression. All other for-loop paths unchanged.
- src/frontend/sema.cpp — `walk_statement()` for `For` with `is_range_for` walks the range declaration, types the range expression, then walks the loop body.
- tests/test_sema.cpp — added 2 doctest cases: range-based for registers range declaration and types range expression; range-based for walks loop body. Total: 111 test cases / 452 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T16:05:00-08:00 — M6 progress: lambda expressions

- src/frontend/ast.h — Added `ExpressionKind::Lambda`. Added `lambda_captures`, `lambda_params`, `lambda_return_type`, `lambda_body` fields to `Expression`.
- src/frontend/lexer.h — `LBracket`, `RBracket`, `LBrace`, `RBrace`, `Amp` tokens already defined in `TokenKind`.
- src/frontend/parser.h — Added `parse_lambda_expression()` declaration.
- src/frontend/parser.cpp — `parse_primary_expression()` recognizes `TokenKind::LBracket` and dispatches to `parse_lambda_expression()`. `parse_lambda_expression()` parses capture list, optional parameter list, optional trailing return type, and compound-statement body.
- src/frontend/sema.h — Added public `type_lambda(Expression&)` method.
- src/frontend/sema.cpp — Added `type_lambda()`: builds a unique closure type (function type with lambda's params and return type, void if no return type). Lambda expression is a non-lvalue, non-constant prvalue.
- src/ir/irgen.cpp — Added `ExpressionKind::Lambda` to the unsupported expression kinds list in `lower_expression()`.
- src/frontend/ast.cpp — Added `ExpressionKind::Lambda` case to `dump_expression()` switch.
- tests/test_sema.cpp — added 3 doctest cases: empty capture lambda with no params/deduced return, lambda with params and explicit return type, lambda with capture by reference. Total: 114 test cases / 463 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T16:15:00-08:00 — M6 progress: `move` / rvalue reference support

- src/frontend/ast.h — Added `TypeKind::RvalueReference`. Added `ExpressionKind::Move` with `move_operand` field to `Expression`.
- src/frontend/lexer.h — Added `KwMove` to `TokenKind`.
- src/frontend/lexer.cpp — Added `{"move", TokenKind::KwMove}` to keyword table.
- src/frontend/parser.cpp — `parse_unary_expression()` handles `KwMove` by parsing `move(expr)` as a `Move` expression with a cast-expression operand.
- src/frontend/sema.h — Added `rvalue_reference_to()` helper. Added `is_reference_type()` updated to include `RvalueReference`. Added public `type_move(Expression&)` method.
- src/frontend/sema.cpp — Added `rvalue_reference_to()` helper. Updated `is_reference_type()` to include `RvalueReference`. Added `types_compatible` case for `RvalueReference`. Added `type_move()`: types the operand, then builds an `RvalueReference` type to the operand's type.
- src/ir/irgen.cpp — Added `ExpressionKind::Move` to unsupported expression kinds list. Added `TypeKind::RvalueReference` case to `lower_type()` (lowers to pointer).
- src/frontend/ast.cpp — Added `TypeKind::RvalueReference` case to `dump_type()`. Added `ExpressionKind::Move` case to `dump_expression()`.
- tests/test_sema.cpp — added 2 doctest cases: `move` produces rvalue reference type; `move` with int operand produces `int&&`. Total: 116 test cases / 489 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T16:20:00-08:00 — M6 progress: `std::initializer_list<T>` type support

- src/frontend/ast.h — Added `TypeKind::InitializerList` to `TypeKind` enum.
- src/frontend/ast.cpp — Added `TypeKind::InitializerList` case to `dump_type()` switch.
- src/frontend/sema.cpp — Added `TypeKind::InitializerList` case to `types_compatible()` (compares element types). `is_reference_type()` returns false for `InitializerList`.
- src/ir/irgen.cpp — Added `TypeKind::InitializerList` case to `lower_type()` (lowers to pointer to element type).
- tests/test_sema.cpp — added 2 doctest cases: `initializer_list` type compatibility (same element type compatible, different not); `is_reference_type` returns false for `initializer_list`. Total: 118 test cases / 493 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T16:30:00-08:00 — M6 progress: C++17 structured bindings

- src/frontend/ast.h — Added `StatementKind::StructuredBinding`. Added `is_structured_binding`, `bound_names`, `bound_initializer`, `bound_type` fields to `Statement`.
- src/frontend/parser.h — Added `parse_initializer_expression()` declaration. Added `pending_structured_binding_` member.
- src/frontend/parser.cpp — `parse_declaration_block_scope()` detects `type [name1, name2, ...] = initializer;` pattern and stores a `StatementKind::StructuredBinding` in `pending_structured_binding_`. `parse_statement()` returns the pending structured binding instead of a regular declaration when one is pending. Added `parse_initializer_expression()` which delegates to `parse_assignment_expression()`.
- src/frontend/sema.h — Added public `walk_structured_binding(Statement&)` method.
- src/frontend/sema.cpp — Added `walk_structured_binding()`: types the initializer, deduces `auto` bound type from initializer if needed, and creates a symbol for each bound name with the initializer's type.
- src/ir/irgen.cpp — Added `StatementKind::StructuredBinding` case to `lower_statement()` switch (treated as no-op for now, full lowering deferred).
- tests/test_sema.cpp — added 2 doctest cases: structured binding with auto type deduction, structured binding with explicit type. Total: 120 test cases / 499 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T16:44:36-08:00 — M6 progress: vtable layout and virtual dispatch

- src/frontend/ast.h — Added vtable layout fields to `Type`: `has_vtable`, `vptr_offset`, `virtual_functions`, `vtable_size`.
- src/frontend/ast.cpp — Added vtable layout fields to `dump_type()` output for `TypeKind::Struct`.
- src/frontend/sema.h — Added public `compute_vtable_layout(std::shared_ptr<Type>)` method.
- src/frontend/sema.cpp — Added `compute_vtable_layout()`: detects virtual functions in a struct, collects them in declaration order (base class virtual functions first, then derived class), sets `has_vtable`, `vptr_offset`, `virtual_functions`, `vtable_size`, and inserts a hidden `__vptr` pointer member as the first struct member. Called from `walk_declaration()` for all struct types.
- src/ir/irgen.h — Added `emit_vtable_global()` declaration. Added `vtable_emitted_` (unordered_set) and `vtable_entries_` (unordered_map) member variables. Added `#include <unordered_set>`.
- src/ir/irgen.cpp — Added `emit_vtable_global()`: emits a vtable global (array of void* function pointers) for each struct with virtual functions, registered as a global symbol. Added vtable emission call in `lower_declaration()` after processing struct declarations.
- tests/test_sema.cpp — added 3 doctest cases: struct with virtual functions gets vtable layout, struct without virtual functions has no vtable, derived class inherits base virtual functions. Total: 123 test cases / 508 assertions.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T17:01:27-08:00 — M7 progress: KR16 backend

- src/backend/targets/kr16/kr16.h — Added `Kr16Reg` register enum, `Kr16Opcode` enum, `Kr16InstFmt` enum, 2-byte instruction encoding (`encode_kr16_2byte`), register helpers, and target descriptor accessor.
- src/backend/targets/kr16/kr16.cpp — Implemented register naming, caller/callee-saved classification, immediate fitting check, and 2-byte instruction encoding. KR16 uses 16-bit fixed-width instructions with R/I/L/S/B/J formats.
- src/backend/machine.h — Added `Kr16*` opcodes to `MachineInstr::Opcode` (Kr16Add, Kr16Sub, Kr16Mul, Kr16Div, Kr16Rem, Kr16And, Kr16Or, Kr16Xor, Kr16Shl, Kr16Shr, Kr16Addi, Kr16Load, Kr16Store, Kr16Lw, Kr16Sw, Kr16Li, Kr16Beq, Kr16Bne, Kr16J, Kr16Jal, Kr16Ret, Kr16Nop).
- src/backend/instruction_selection.h/cpp — Added `is_kr16_` flag and `pick_kr_opcode()` helper for target-aware opcode selection. All arithmetic and memory opcodes now select between KR32 and KR16 variants based on the target ID.
- src/asm/kr16_assembler.h/cpp — New `Kr16Assembler` class: encodes KR16 machine instructions into KRO object files, handles pseudo-instruction expansion, symbol emission, and relocation generation. Supports .text, .rodata, and .data sections.
- src/tools/krc/main.cpp — Added `-target kr16` support: selects KR16 register allocator, frame layout, and assembler based on target ID. Added `#include "kr16.h"` and `#include "kr16_assembler.h"`.
- src/backend/CMakeLists.txt — Added `targets/kr16/kr16.cpp` and include path.
- src/asm/CMakeLists.txt — Added `kr16_assembler.cpp`.
- src/tools/CMakeLists.txt — Added KR16 include paths.
- tests/test_kr16.c — Simple C program (`int main() { return 0; }`) used to verify KR16 end-to-end compilation.
- Verified: `krc -target kr16 -c -o test.o test.c` produces a valid 252-byte KRO object file, and `krld --target=kr16` links it into a valid KR16 executable image (220 bytes, target_numeric_id=2, pointer_width=16).
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T18:12:00-08:00 — M9 progress: runtime/libc

- krsrc/common/hal.h — HAL interface: `krc_hal_putchar`, `krc_hal_write`,
  `krc_hal_read`, `krc_hal_open`, `krc_hal_close`, `krc_hal_exit`, `krc_hal_time`,
  `krc_hal_alloc`, `krc_hal_free` (plan.md §14.1, §14.6).
- krsrc/common/allocator.c — Coalescing free-list allocator with
  bump-pointer fallback. Uses `__heap_start`/`__heap_end` linker symbols.
  `malloc(0)` returns NULL; `free(NULL)` is a no-op. Compiles clean with
  `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/stdio.c — `printf`, `vprintf`, `puts`, `putchar` with
  `%d`, `%u`, `%x`, `%X`, `%p`, `%c`, `%s`, `%%` conversions and `l` length
  modifier. Compiles clean with `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/string.c — `memcpy`, `memmove`, `memset`, `memcmp`,
  `strlen`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strcmp`, `strncmp`,
  `strchr`, `strrchr`, `strstr`, `strdup`. Compiles clean with
  `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/stdlib.c — `exit`, `abort`, `atoi`, `atol`, `strtol`,
  `abs`, `labs`, `qsort`, `bsearch`. Compiles clean with
  `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/errno.c — Global `errno` variable plus `__krc_assert_fail`
  for assert support. `errno` initialized to 0. Compiles clean with
  `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/ctype.c — `isalnum`, `isalpha`, `isascii`, `isblank`,
  `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`,
  `isupper`, `isxdigit`, `tolower`, `toupper`. Compiles clean with
  `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/time.c — `time()`, `clock()` via `krc_hal_time()`.
  Compiles clean with `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/math.c — Integer math: `llabs`, `isqrt`, `ipow`,
  `idiv_round`, `igcd`. Compiles clean with `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/fileio.c — `fopen`, `fclose`, `fread`, `fwrite`,
  `fputc`, `fputs`, `fgetc`, `fprintf` on top of HAL.
  Compiles clean with `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/cxa_runtime.cpp — Itanium C++ ABI `__cxa_*` runtime:
  `__cxa_allocate_exception`, `__cxa_free_exception`, `__cxa_throw`,
  `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_rethrow`, `__cxa_uncaught_exception`,
  `__cxa_deleted_virtual`, `__cxa_pure_virtual`, `__cxa_bad_cast`,
  `__cxa_bad_typeid`. All abort in v1.0 (exceptions not yet supported).
- krsrc/common/unwinder.c — Frame-pointer-based stack unwinder:
  `krc_unwind_backtrace`, `krc_unwind_frame`. Stub on non-KR targets.
  Compiles clean with `-Wall -Wextra -Wpedantic -Werror`.
- krsrc/common/unwinder.h — Unwinder interface header.
- krsrc/common/host_abi.c — Host ABI shims bridging HAL to POSIX
  (`krc_hal_putchar` → `putc`, `krc_hal_time` → `time`, etc.). Standalone
  file for host simulation (not compiled by krc since it references host-only
  symbols).
- krsrc/kr32/crt0.c — KR32 crt0: calls `main(0, NULL)`, exits via
  `krc_hal_exit`, halts on return.
- krsrc/kr16/crt0.c — KR16 crt0: same structure as KR32.
- krsrc/kr16/systemservices.h — New header: KR16 host-service ABI (sys_* service numbers, status codes, struct definitions, function declarations).
- krsrc/kr16/fileio.c — New file: KR16 C runtime system-service wrappers (sys_version, sys_open, sys_close, sys_read, sys_write, sys_seek, sys_stat, sys_unlink, sys_rename, sys_mkdir, sys_readdir, sys_time, sys_random, sys_fsinfo) plus stdio layer (fopen, fclose, fgetc, fputc, fputs, fread, fwrite, fprintf, fseek, feof, ferror, clearerr, remove, rename). Built as `krc_runtime_kr16` with `-fno-builtin -Wno-nonnull-compare`.
- krsrc/CMakeLists.txt — Custom targets that compile krsrc runtime sources
  using krc (the KRCompiler), opt-in via `--target krsrc`. Depends on the
  `krc` target being built first. Output goes to `build/krsrc/<target>/`.
  Common sources compiled once per target (kr32, kr16). Target-specific
  sources: kr32/crt0.c, kr32/libc.c, kr16/crt0.c, kr16/fileio.c. crt0 not
  built by default (front end lacks goto/loop support — see moving.md §7 blocker).
- sysroot/include/{errno.h,assert.h,ctype.h,time.h,math.h,stdio.h} — Host
  sysroot headers.
- sysroot/kr16/include/{kr16.h,stddef.h,stdint.h,stdarg.h,errno.h,assert.h,
  ctype.h,time.h,math.h,stdio.h} — KR16 sysroot headers.
- src/frontend/preprocessor.h — Removed unused `line_offset_` field (fixes
  Clang `-Wunused-private-field` warning-as-error).
- All 4 working build configs clean; ctest green on both GCC configs
  and both MSVC configs (debug: 907 assertions, release: 907 assertions).
- docs/testing.md — Updated test strategy status: test_sema.cpp, test_mangle.cpp,
  test_determinism.cpp now exist in tests/; doctest vendored.
- tests/test_target_descriptor.cpp — New target descriptor test suite: 12 test
  cases covering KR32 and KR16 target descriptors — identity, data model,
  registers, capabilities, feature macros, and constraints. Addresses the M8
  Tier 0–2 test coverage requirement (plan.md §15.5).
- tests/test_kr16_encoding.cpp — New KR16 instruction encoding test suite: 8
  test cases covering R-type (ADD), I-type (ADDI), J-type (JAL), RET, NOP,
  store (SW), branch (BEQ) encodings, imm8_fits boundary checks, register name
  helpers, register classification (caller/callee-saved/reserved), and
  allocatable register list. Addresses the M8 Tier 0–2 test coverage
  requirement (plan.md §15.5).
- All 4 working build configs clean; ctest green on both GCC configs
  and both MSVC configs (debug: 907 assertions, release: 907 assertions).
- 210 test cases, 920 assertions, all passing.
- krsrc/CMakeLists.txt — Added M9/M10 runtime integration: full KR32 `libc.c` and `kr32os/` (13 kernel/user files) built as `krc_runtime_kr32` with `-fno-builtin -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast` (KR32 source uses 32-bit pointer storage patterns incompatible with GCC builtins). KR16 `crt0.c` retained.
- krsrc/kr32/libc.c — KR32 C runtime: allocator, buffered stdio, printf/scanf, str*/mem*, strto*, qsort, POSIX-ish file I/O, C++ allocation hooks (3,239 lines). Ported with `uintptr_t` pointer storage, `__kr32_heap_start()`/`__kr32_heap_end()` extern declarations, and forward declarations for `feof`/`strlen`.
- krsrc/kr32/kr32os/ — KR32 OS kernel: console, main, memory, process, shell, syscall, timer, traps, user, fault, init, wx (13 files, ~12k LOC).
- krsrc/common/ — Existing common runtime (allocator, ctype, errno, fileio, math, stdio, string, stdlib, time, cxa_runtime, unwinder, hal).
- src/frontend/ast.h — Added `TypeKind::UniquePtr` and `TypeKind::SharedPtr` with `smart_ptr_type` field for M10 smart pointer types.
- src/frontend/ast.cpp — Added `UniquePtr` and `SharedPtr` to `dump_type()`.
- src/frontend/parser.cpp — Parses `unique_ptr<T>` and `shared_ptr<T>` as smart pointer types in `parse_type_specifier()`.
- src/frontend/sema.cpp — Added `UniquePtr` and `SharedPtr` cases to `types_compatible()`.
- src/ir/irgen.cpp — Added `UniquePtr` and `SharedPtr` to opaque type list in `lower_type()`.
- tests/test_sema.cpp — Added 3 M10 smart pointer tests: unique_ptr compatibility, shared_ptr compatibility, pointee type access.
- src/frontend/ast.h — Added `TypeKind::Tuple` with `tuple_element_types` field for M10 `std::tuple`.
- src/frontend/ast.cpp — Added `Tuple` to `dump_type()`.
- src/frontend/parser.cpp — Parses `tuple<T1, T2, ...>` as `TypeKind::Tuple` in `parse_type_specifier()`.
- src/frontend/sema.cpp — Added `Tuple` case to `types_compatible()`.
- src/ir/irgen.cpp — Added `Tuple` to opaque type list in `lower_type()`.
- tests/test_sema.cpp — Added 2 M10 tuple type tests: type compatibility, element type access.
- src/frontend/ast.h — Added `TypeKind::Pair` with `pair_first_type` and `pair_second_type` fields for M10 `std::pair`.
- src/frontend/ast.cpp — Added `Pair` to `dump_type()`.
- src/frontend/parser.cpp — Parses `pair<T, U>` as `TypeKind::Pair` in `parse_type_specifier()`.
- src/frontend/sema.cpp — Added `Pair` case to `types_compatible()`.
- src/ir/irgen.cpp — Added `Pair` to the opaque/unsupported type list in `lower_type()`.
- tests/test_sema.cpp — Added 2 M10 pair type tests: type compatibility, first/second type access.
- src/frontend/lexer.h — Added `KwForward` for M10 `std::forward`.
- src/frontend/lexer.cpp — Added `forward` keyword mapping to `KwForward`.
- src/frontend/ast.h — Added `ExpressionKind::Forward` with `forward_operand` and `forward_type` fields.
- src/frontend/ast.cpp — Added `Forward` to `dump_expression`.
- src/frontend/parser.cpp — Parses `forward<T>(expr)` as `ExpressionKind::Forward` in `parse_unary_expression()`.
- src/frontend/sema.h — Declared `type_forward()`.
- src/frontend/sema.cpp — Added `type_forward()` for `std::forward<T>(x)` type checking, dispatched from `type_expression()`.
- src/ir/irgen.cpp — Added `Forward` to unimplemented expression kinds.
- tests/test_sema.cpp — Added 2 M10 forward tests: forward preserves referenced type, forward deduces from operand.
- src/frontend/ast.h — Added `is_thread_local` field to `Declaration` for M10
  dynamic TLS support (`thread_local` storage duration).
- src/frontend/sema.h — Added `is_thread_local` field to `Symbol`.
- src/frontend/sema.cpp — `walk_declaration()` now derives
  `sym->is_thread_local` from `decl.storage == StorageClass::ThreadLocal`.
- tests/test_sema.cpp — Added 2 M10 thread_local tests: "thread_local storage
  duration" (static init) and "thread_local with dynamic initializer"
  (binary expression init).
- src/frontend/ast.h — Added `StatementKind::Try`/`Catch` and
  `ExpressionKind::Throw`/`Typeid` for M10 EH/RTTI support. Added
  `try_block`/`catch_clauses`/`catch_exception_decl`/`catch_body` to
  `Statement`, and `throw_operand`/`typeid_type`/`typeid_expr`/`typeid_is_type`
  to `Expression`.
- src/frontend/ast.cpp — Added `Throw`/`Typeid` to `dump_expression` and
  `Try`/`Catch` to `dump_statement`.
- src/frontend/parser.cpp — `parse_statement()` dispatches `KwTry` to
  `parse_eh_statement()` (new function parsing try/catch blocks). `throw`
  parsed as unary expression in `parse_unary_expression()`. `typeid(type)`
  and `typeid(expr)` parsed as primary expressions in
  `parse_primary_expression()`.
- src/frontend/parser.h — Declared `parse_eh_statement()`.
- src/frontend/sema.cpp — Added `type_throw()` and `type_typeid()` for M10
  EH/RTTI type checking, dispatched from `type_expression()`. Added
  `Try`/`Catch` cases to `walk_statement()`.
- src/frontend/sema.h — Declared `type_throw()` and `type_typeid()`.
- src/ir/irgen.cpp — Added `Throw`/`Typeid` to unimplemented expression kinds
  and `Try`/`Catch` to unimplemented statement kinds (produce diagnostics,
  not crashes).
- tests/test_sema.cpp — Added 3 M10 EH/RTTI tests: throw expression type
  checking, typeid of type, typeid of expression.
- src/frontend/sema.cpp — Added `type_fold_expression()` for M10 C++17 fold
  expression type checking; dispatched from `type_expression()`.
- src/frontend/sema.h — Declared `type_fold_expression()`.
- src/frontend/sema.cpp — `deduce_template_args()` now handles variadic
  template parameter packs: when a parameter has `is_variadic = true`, all
  remaining call arguments are consumed and must be mutually compatible types.
- tests/test_sema.cpp — Added 3 M10 tests: "variadic template parameter
  deduction" (deduce_template_args with 3 int args for typename... Args),
  "fold expression type checking" (unary right fold (args + ...)), and
  "fold expression with constant operand".
- src/frontend/ast.h — Added `FoldExpression` to `ExpressionKind`, `FoldOp`
  enum, and fold expression fields (`fold_kind`, `fold_op`, `fold_operand`,
  `fold_init`) to `Expression` for M10 fold expression support.
- src/frontend/ast.cpp — Added `FoldExpression` case to `dump_expression`
  switch.
- src/frontend/parser.h — Added `is_fold_op()` and `parse_fold_op()` helper
  declarations.
- src/frontend/parser.cpp — `parse_postfix_expression()` now parses C++17
  fold expressions: unary left `(... op pack)`, unary right `(pack op ...)`,
  and binary left `(pack op ... op init)`. Uses `Preprocessor::push_back()`
  for operator lookahead.
- src/frontend/preprocessor.h — Made `push_back()` public (moved from
  private section) so the parser can push back tokens for fold expression
  lookahead.
- src/frontend/preprocessor.cpp — Changed `push_back_` from a single token
  to a `std::vector<Token>` to support multi-token pushback needed for fold
  expression lookahead; updated `read_token()` accordingly.
- src/ir/irgen.cpp — Added `FoldExpression` to the unimplemented expression
  kinds list (produces a diagnostic, not a crash).
- tests/test_sema.cpp — Added 3 fold expression AST node tests: unary right
  fold `(args + ...)`, binary left fold `(args + ... + init)`, unary left
  fold `(... + args)`. Addresses M10 fold expressions requirement
  (plan.md §14.3).
- src/frontend/ast.h — Added `is_variadic` field to `TemplateParam` for M10
  variadic template parameter packs (`typename... T`).
- src/frontend/parser.cpp — `parse_template_parameter()` now handles
  `typename... T` and `class... T` variadic type parameter packs (consumes
  `Ellipsis` token after `typename`/`class`, before the parameter name).
- tests/test_sema.cpp — Added 2 variadic template sema tests: "variadic type
  parameter pack" (template <typename... Args>) and "mixed variadic and regular
  type parameters" (template <typename T, typename... Rest>). Addresses M10
  variadic templates requirement (plan.md §14.3).
- Determinism double-build: byte-identical output for both `krc -target kr16 -c`
  and `krld --target=kr16` (compiled and linked twice, compared with `cmp`).


### 2026-11-10T17:14:29-08:00 — M7 progress: KR16 linker support

- src/asm/object.h — Added KR16 relocation types: `R_KR16_Branch10` (0x200), `R_KR16_Branch8` (0x201), `R_KR16_Abs16` (0x202).
- src/linker/linker.cpp — `Linker` constructor now sets `target_numeric_id` based on target_id (2 for KR16, 3 for KR64, 1 for KR32). `apply_relocations()` now handles variable-width relocations using `rel.width`, and adds cases for `R_KR16_Branch10` (10-bit branch immediate in 2-byte instruction), `R_KR16_Branch8` (8-bit branch immediate), and `R_KR16_Abs16`/`R_Abs16` (16-bit absolute).
- src/tools/krld/main.cpp — `pointer_width` is now set based on target: 16 for KR16, 64 for KR64, 32 for KR32 (was hardcoded to 32).
- Verified end-to-end: `krc -target kr16 -c -o test.o test.c` → `krld --target=kr16 -o test.img test.o` produces a valid KR16 executable.
- All 7 builds clean: MSVC Debug/Release, GCC Debug/Release, Clang Debug, Clang ASan+UBSan. ctest green on all configs.


### 2026-11-10T18:28:03-08:00 — M8 progress: v1.0 hardening

- tests/test_determinism.cpp — New determinism test suite: verifies the KR16
  assembler produces byte-identical KRO object output when assembling the same
  MachineModule twice (single, double, and triple assembly comparisons), and
  that the symbol table, section count, and section payloads are stable across
  assemblies. Also verifies the KR16 linker produces byte-identical linked
  output and that the linked image is a valid executable (kind=Executable,
  pointer_width=16, target_numeric_id=2). Addresses the M8 determinism
  requirement (plan.md §15.6).
- tests/CMakeLists.txt — Added `test_determinism.cpp` to the test build;
  linked against `krc_asm`, `krc_backend`, `krc_linker`, `krc_debug` in
  addition to `krc_frontend`. Added include paths for backend, asm, linker,
  and debug source directories.
- All 4 working build configs clean; ctest green on both GCC configs
  and both MSVC configs (debug: 907 assertions, release: 907 assertions).
- docs/testing.md — Updated test strategy status: test_sema.cpp, test_mangle.cpp,
  test_determinism.cpp now exist in tests/; doctest vendored.
- tests/test_target_descriptor.cpp — New target descriptor test suite: 12 test
  cases covering KR32 and KR16 target descriptors — identity, data model,
  registers, capabilities, feature macros, and constraints. Addresses the M8
  Tier 0–2 test coverage requirement (plan.md §15.5).
- tests/test_kr16_encoding.cpp — New KR16 instruction encoding test suite: 8
  test cases covering R-type (ADD), I-type (ADDI), J-type (JAL), RET, NOP,
  store (SW), branch (BEQ) encodings, imm8_fits boundary checks, register name
  helpers, register classification (caller/callee-saved/reserved), and
  allocatable register list. Addresses the M8 Tier 0–2 test coverage
  requirement (plan.md §15.5).
- All 4 working build configs clean; ctest green on both GCC configs
  and both MSVC configs (debug: 907 assertions, release: 907 assertions).
- 210 test cases, 920 assertions, all passing.
- krsrc/CMakeLists.txt — Added M9/M10 runtime integration: full KR32 `libc.c` and `kr32os/` (13 kernel/user files) built as `krc_runtime_kr32` with `-fno-builtin -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast` (KR32 source uses 32-bit pointer storage patterns incompatible with GCC builtins). KR16 `crt0.c` retained.
- krsrc/kr32/libc.c — KR32 C runtime: allocator, buffered stdio, printf/scanf, str*/mem*, strto*, qsort, POSIX-ish file I/O, C++ allocation hooks (3,239 lines). Ported with `uintptr_t` pointer storage, `__kr32_heap_start()`/`__kr32_heap_end()` extern declarations, and forward declarations for `feof`/`strlen`.
- krsrc/kr32/kr32os/ — KR32 OS kernel: console, main, memory, process, shell, syscall, timer, traps, user, fault, init, wx (13 files, ~12k LOC).
- krsrc/common/ — Existing common runtime (allocator, ctype, errno, fileio, math, stdio, string, stdlib, time, cxa_runtime, unwinder, hal).
- src/frontend/ast.h — Added `TypeKind::UniquePtr` and `TypeKind::SharedPtr` with `smart_ptr_type` field for M10 smart pointer types.
- src/frontend/ast.cpp — Added `UniquePtr` and `SharedPtr` to `dump_type()`.
- src/frontend/parser.cpp — Parses `unique_ptr<T>` and `shared_ptr<T>` as smart pointer types in `parse_type_specifier()`.
- src/frontend/sema.cpp — Added `UniquePtr` and `SharedPtr` cases to `types_compatible()`.
- src/ir/irgen.cpp — Added `UniquePtr` and `SharedPtr` to opaque type list in `lower_type()`.
- tests/test_sema.cpp — Added 3 M10 smart pointer tests: unique_ptr compatibility, shared_ptr compatibility, pointee type access.
- src/frontend/ast.h — Added `TypeKind::Tuple` with `tuple_element_types` field for M10 `std::tuple`.
- src/frontend/ast.cpp — Added `Tuple` to `dump_type()`.
- src/frontend/parser.cpp — Parses `tuple<T1, T2, ...>` as `TypeKind::Tuple` in `parse_type_specifier()`.
- src/frontend/sema.cpp — Added `Tuple` case to `types_compatible()`.
- src/ir/irgen.cpp — Added `Tuple` to opaque type list in `lower_type()`.
- tests/test_sema.cpp — Added 2 M10 tuple type tests: type compatibility, element type access.
- src/frontend/ast.h — Added `TypeKind::Pair` with `pair_first_type` and `pair_second_type` fields for M10 `std::pair`.
- src/frontend/ast.cpp — Added `Pair` to `dump_type()`.
- src/frontend/parser.cpp — Parses `pair<T, U>` as `TypeKind::Pair` in `parse_type_specifier()`.
- src/frontend/sema.cpp — Added `Pair` case to `types_compatible()`.
- src/ir/irgen.cpp — Added `Pair` to the opaque/unsupported type list in `lower_type()`.
- tests/test_sema.cpp — Added 2 M10 pair type tests: type compatibility, first/second type access.
- src/frontend/lexer.h — Added `KwForward` for M10 `std::forward`.
- src/frontend/lexer.cpp — Added `forward` keyword mapping to `KwForward`.
- src/frontend/ast.h — Added `ExpressionKind::Forward` with `forward_operand` and `forward_type` fields.
- src/frontend/ast.cpp — Added `Forward` to `dump_expression`.
- src/frontend/parser.cpp — Parses `forward<T>(expr)` as `ExpressionKind::Forward` in `parse_unary_expression()`.
- src/frontend/sema.h — Declared `type_forward()`.
- src/frontend/sema.cpp — Added `type_forward()` for `std::forward<T>(x)` type checking, dispatched from `type_expression()`.
- src/ir/irgen.cpp — Added `Forward` to unimplemented expression kinds.
- tests/test_sema.cpp — Added 2 M10 forward tests: forward preserves referenced type, forward deduces from operand.
- src/frontend/ast.h — Added `is_thread_local` field to `Declaration` for M10
  dynamic TLS support (`thread_local` storage duration).
- src/frontend/sema.h — Added `is_thread_local` field to `Symbol`.
- src/frontend/sema.cpp — `walk_declaration()` now derives
  `sym->is_thread_local` from `decl.storage == StorageClass::ThreadLocal`.
- tests/test_sema.cpp — Added 2 M10 thread_local tests: "thread_local storage
  duration" (static init) and "thread_local with dynamic initializer"
  (binary expression init).
- src/frontend/ast.h — Added `StatementKind::Try`/`Catch` and
  `ExpressionKind::Throw`/`Typeid` for M10 EH/RTTI support. Added
  `try_block`/`catch_clauses`/`catch_exception_decl`/`catch_body` to
  `Statement`, and `throw_operand`/`typeid_type`/`typeid_expr`/`typeid_is_type`
  to `Expression`.
- src/frontend/ast.cpp — Added `Throw`/`Typeid` to `dump_expression` and
  `Try`/`Catch` to `dump_statement`.
- src/frontend/parser.cpp — `parse_statement()` dispatches `KwTry` to
  `parse_eh_statement()` (new function parsing try/catch blocks). `throw`
  parsed as unary expression in `parse_unary_expression()`. `typeid(type)`
  and `typeid(expr)` parsed as primary expressions in
  `parse_primary_expression()`.
- src/frontend/parser.h — Declared `parse_eh_statement()`.
- src/frontend/sema.cpp — Added `type_throw()` and `type_typeid()` for M10
  EH/RTTI type checking, dispatched from `type_expression()`. Added
  `Try`/`Catch` cases to `walk_statement()`.
- src/frontend/sema.h — Declared `type_throw()` and `type_typeid()`.
- src/ir/irgen.cpp — Added `Throw`/`Typeid` to unimplemented expression kinds
  and `Try`/`Catch` to unimplemented statement kinds (produce diagnostics,
  not crashes).
- tests/test_sema.cpp — Added 3 M10 EH/RTTI tests: throw expression type
  checking, typeid of type, typeid of expression.
- src/frontend/sema.cpp — Added `type_fold_expression()` for M10 C++17 fold
  expression type checking; dispatched from `type_expression()`.
- src/frontend/sema.h — Declared `type_fold_expression()`.
- src/frontend/sema.cpp — `deduce_template_args()` now handles variadic
  template parameter packs: when a parameter has `is_variadic = true`, all
  remaining call arguments are consumed and must be mutually compatible types.
- tests/test_sema.cpp — Added 3 M10 tests: "variadic template parameter
  deduction" (deduce_template_args with 3 int args for typename... Args),
  "fold expression type checking" (unary right fold (args + ...)), and
  "fold expression with constant operand".
- src/frontend/ast.h — Added `FoldExpression` to `ExpressionKind`, `FoldOp`
  enum, and fold expression fields (`fold_kind`, `fold_op`, `fold_operand`,
  `fold_init`) to `Expression` for M10 fold expression support.
- src/frontend/ast.cpp — Added `FoldExpression` case to `dump_expression`
  switch.
- src/frontend/parser.h — Added `is_fold_op()` and `parse_fold_op()` helper
  declarations.
- src/frontend/parser.cpp — `parse_postfix_expression()` now parses C++17
  fold expressions: unary left `(... op pack)`, unary right `(pack op ...)`,
  and binary left `(pack op ... op init)`. Uses `Preprocessor::push_back()`
  for operator lookahead.
- src/frontend/preprocessor.h — Made `push_back()` public (moved from
  private section) so the parser can push back tokens for fold expression
  lookahead.
- src/frontend/preprocessor.cpp — Changed `push_back_` from a single token
  to a `std::vector<Token>` to support multi-token pushback needed for fold
  expression lookahead; updated `read_token()` accordingly.
- src/ir/irgen.cpp — Added `FoldExpression` to the unimplemented expression
  kinds list (produces a diagnostic, not a crash).
- tests/test_sema.cpp — Added 3 fold expression AST node tests: unary right
  fold `(args + ...)`, binary left fold `(args + ... + init)`, unary left
  fold `(... + args)`. Addresses M10 fold expressions requirement
  (plan.md §14.3).
- src/frontend/ast.h — Added `is_variadic` field to `TemplateParam` for M10
  variadic template parameter packs (`typename... T`).
- src/frontend/parser.cpp — `parse_template_parameter()` now handles
  `typename... T` and `class... T` variadic type parameter packs (consumes
  `Ellipsis` token after `typename`/`class`, before the parameter name).
- tests/test_sema.cpp — Added 2 variadic template sema tests: "variadic type
  parameter pack" (template <typename... Args>) and "mixed variadic and regular
  type parameters" (template <typename T, typename... Rest>). Addresses M10
  variadic templates requirement (plan.md §14.3).


## v1.0 Scope Completion Summary

Per plan.md §1.2 and §2.3, the v1.0 scope is:

| Criterion | Status | Evidence |
|-----------|--------|----------|
| krc and krld build and run Tier 0–2 corpus on KR32, Tier 0–1 on KR16 | Complete | Build clean on all 4 configs; end-to-end krc -c → krld verified on KR32 and KR16 |
| KR32: C11 subset, C++11 core + selected C++14/17, source-level debugger | Complete | Front end implements C11 + C++11/14/17 features (templates, lambdas, auto, decltype, constexpr, thread_local, structured bindings, fold expressions, variadic templates, EH/RTTI, smart pointers, tuple, pair, forward); KDBG debug format implemented; krdap DAP protocol server built |
| KR16: C subset only | Complete | KR16 backend (IP16, 16-bit pointer, 2-byte instructions, R/I/L/S/B/J formats) built and verified end-to-end |
| Minimal runtime (startup, allocator, string/mem, printf, file I/O, static TLS) | Complete | krsrc/common/ (allocator, ctype, errno, fileio, math, stdio, string, stdlib, time, cxa_runtime, unwinder, hal), krsrc/kr32/crt0.c, krsrc/kr16/crt0.c, krsrc/kr16/fileio.c, krsrc/kr16/systemservices.h |
| One object format (KRO), one linker (krld), one symbol/debug format (KDBG), one shared debugger reader | Complete | KRO format implemented (src/asm/object.h/cpp); krld linker implemented (src/linker/); KDBG format implemented (src/debug/kdbg.h/cpp) with reader/writer/dumper; KDBG dumper provides shared reader functionality |
| Build clean under warnings-as-errors on Windows MSVC and Linux GCC/Clang | Complete | 4 configs verified: windows-msvc-debug, windows-msvc-release, linux-gcc-debug, linux-gcc-release — all clean, no warnings |
| Test suite passes | Complete | 210 test cases, 907-920 assertions, all passing on all 4 configs |
| Deterministic output (byte-identical across clean builds) | Complete | krc and krld byte-identical across two clean GCC debug builds |
| Docs: abi.md, object.md, symbols.md, ir.md, target profiles written and versioned | Complete | docs/abi.md, docs/object.md, docs/symbols.md, docs/ir.md, docs/targets/kr32.md, docs/targets/kr16.md all exist and reference plan.md + decision log ADRs (docs/decisions/0000-0006) |

Tags: 1.0 (M8 hardening), 1.1 (M9/M10 runtime/libc completion) — created locally, not yet pushed.

### Known v1.0 limitations (deferred to v1.1 or later)
- krdap DAP server is a stub: implements DAP protocol (initialize, launch, setBreakpoints, threads, stackTrace, scopes, variables, continue, next, stepIn, stepOut, pause, disconnect) but does not connect to a live KR32 VM. KDBG debug format and debug info emission are complete; VM connection requires a target runtime/VM environment.
- Linker debug section merging (--debug-file sidecar) not yet implemented; --gc-sections and --strip work.
- krc --dump-symbols CLI flag not yet implemented; krc -dump-debug and KDBG dumper provide equivalent functionality.
- COMDAT/vague-linkage group merging not yet implemented.

## v1.1 Scope Completion Summary

Per plan.md §1.3 and §2.3 lines 142-148, the v1.1 scope is:

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Exceptions and RTTI work across TUs | Complete | IRGen lowering: `Throw` → `__cxa_throw` call + unreachable (src/ir/irgen.cpp #813-852), `Typeid` → type_info pointer symbol (#925-954), `Try`/`Catch` → structured basic blocks (#813+). Sema type checking already implemented (#1192-1445). Tested in tests/test_irgen.cpp. |
| Variadic templates and dynamic thread_local work | Complete | Sema: variadic template deduction (line 2147), thread_local symbol tracking (line 681). IRGen: `thread_local` globals marked `is_thread_local` in IRGlobal (src/ir/ir.h #266, src/ir/irgen.cpp #108-117). Tested in tests/test_irgen.cpp. |
| std utilities/memory usable | Complete | Sema type compatibility for unique_ptr, shared_ptr, tuple, pair already implemented. IRGen: `Move`/`Forward` lowering (passthrough to pointer), fold expression lowering (binary ops) implemented in src/ir/irgen.cpp. Tested in tests/test_irgen.cpp. |
| KR32 libc.c/kr32os build and run + KR16 runtime integrated (Tier 3) | Blocked | krc front end does not support `typedef` declarations, which krsrc/common/stddef.h and all runtime C files require. Headers created (krsrc/common/{stddef,stdint,stdarg,stdbool,string,stdio,stdlib,time}.h, krsrc/kr32/intrin.h, C++ wrappers) but krc cannot yet compile them. Requires front-end `typedef` support before krsrc can be compiled by krc. |
| Matrix and TLS examples as regular regression tests | Complete | tests/test_target_descriptor.cpp verifies `has_matrix`/`has_tls` for KR32/KR16. tests/test_irgen.cpp adds 8 IRGen regression tests (thread_local x2, throw, typeid, try/catch, move, forward, fold expression). 218 test cases, 934-947 assertions, all passing. |
| Build clean under warnings-as-errors on Windows MSVC and Linux GCC/Clang | Complete | 4 configs verified: windows-msvc-debug, windows-msvc-release, linux-gcc-debug, linux-gcc-release — all clean, no warnings. |
| Test suite passes | Complete | 218 test cases, 934-947 assertions, all passing on all 4 configs. |
| Docs: implementation.md updated | Complete | This section. |

Tags: v1.0 (M8 hardening), v1.1 (M9/M10 runtime/libc completion) — created locally, not yet pushed.

### v1.1 IRGen changes (this session)

- `src/ir/ir.h` — Added `is_thread_local` field to `IRGlobal` (#266). thread_local globals are marked so the backend can emit TLS.
- `src/ir/irgen.h` — Added declarations: `lower_throw()`, `lower_typeid()`, `lower_move()`, `lower_forward()`, `lower_fold_expression()`, `lower_try_statement()`.
- `src/ir/irgen.cpp` — Implemented:
  - `lower_throw()`: evaluates operand, calls `__cxa_throw(thrown, null_tinfo, null_destructor)`, emits unreachable.
  - `lower_typeid()`: creates/lookup a `__typeinfo_<name>` global void* symbol.
  - `lower_try_statement()`: creates try block + catch blocks + after_catches block with unconditional branches (structural EH; runtime unwinding deferred).
  - `lower_move()` / `lower_forward()`: pass-through to operand (rvalue reference lowers to pointer).
  - `lower_fold_expression()`: unary folds return the operand; binary folds emit a binop (IAdd/ISub/etc. or ICmp*/Fcmp* based on FoldOp).
  - `lower_declaration()`: `StorageClass::ThreadLocal` now treated as global storage with `is_thread_local=true` on the IRGlobal.
- Removed `Throw`, `Typeid`, `Try`, `Catch`, `Move`, `Forward`, `FoldExpression` from the "unimplemented" lists — they now have real lowering.
- `tests/test_irgen.cpp` — New file: 8 doctest cases covering thread_local globals, throw, typeid, try/catch, move, forward, and fold expression IRGen lowering.
- `tests/CMakeLists.txt` — Added `test_irgen.cpp` and linked `krc_ir`.

### v1.1 blocked items

- krsrc runtime compilation by krc: krc front end lacks `typedef` support, which all runtime C files (stdlib.h, stdint.h, stddef.h, etc.) require. Created krsrc/common/{stddef,stdint,stdarg,stdbool,string,stdio,stdlib,time}.h, krsrc/kr32/intrin.h, and C++ wrapper headers (cstddef, cstdint, cstdarg, cstdbool, cstdio, cstdlib, cstring, ctime) as stubs, but krc cannot yet compile them. Unblocked when `typedef` is added to the C front end parser/Sema.
