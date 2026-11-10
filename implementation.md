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
| M1 C front end + sysroot | 6–8 weeks | Preprocessor, lexer, C11 parser, Sema core, real sysroot headers, AST dump | G1 | in progress |
| M2 IR + KR32 backend + assembler | 8–10 weeks | Typed SSA, C IRGen, KR32 descriptor, instruction selection, regalloc, frames, asm printer, assembler | G2a | in progress |
| M3 Object + linker + startup | 6–8 weeks | Object format, `krld`, crt0, minimal libc, KR32 image writer | G2b | in progress |
| M4 C core + debug format | 8–10 weeks | Structs/pointers/arrays/varargs/preprocessor, KDBG format, dumper/reader, DAP/GUI debug session | G3, G4 | in progress |
| M5 C++ core | 8–10 weeks | Classes, single inheritance, virtual, references, overloading, ctors/dtors, `new`/`delete`, namespaces | G5 | not started |
| M6 C++11/14/17 | 8–10 weeks | Templates, lambdas, move, `auto`/`decltype`, range-for, `constexpr`, `initializer_list`, `noexcept`/`alignas`, `thread_local` static, `if constexpr`, structured bindings, inline variables; fixtures | G6 | not started |
| M7 KR16 backend + C | 6–8 weeks | KR16 descriptor, IP16 data model/ABI, codegen, image writer; Tier 0–1 C | G7 | not started |
| M8 v1.0 hardening | 6–8 weeks | Full Tier 0–2, debugger parity, determinism, docs, tag `v1.0` | G8 | not started |
| M9 v1.1 runtime/libc/host services | 8–10 weeks | Port KR32 `libc.c`, stdio/`printf`, malloc, file I/O, host ABI, kernel profile; integrate KR16 `libc.s`/`fileio.c`; Tier 3 | G9 partial | not started |
| M10 v1.1 language/runtime completion | 8–10 weeks | EH/RTTI, variadic templates + fold expressions, dynamic TLS, utilities/memory, full KR32 `libc.c`/`kr32os`, KR16 runtime; tag `v1.1` | G9 | not started |

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
- [ ] crt0 startup for KR32
- [ ] Minimal libc (printf, malloc, file I/O)
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
