# kevinc — implementation plan

Status: **delivered fixture** (2026-11-12T11:07:08-08:00). Owner: the
`kevinc/` agent session.
Read `README.md` first; this directory is closed to other sessions.

## 1. Purpose

`kevinc.cpp` is a single-file, C++17 implementation of a C89 compiler that
targets the KR32 virtual CPU. It exists for two reasons:

1. It is a realistic, large C++17 translation unit for testing `KRCompilerN`'s
   ability to parse modern C++ — templates, variants, optional, structured
   bindings, `constexpr`, generic lambdas, fold expressions, and the standard
   library.
2. It is a working end-to-end compiler shape: source text in, KR32 assembly
   (`.S`) out. It is **not** part of `krc`, has no build-system integration,
   and is not linked into anything.

Correctness of the emitted program is not an acceptance criterion. The
acceptance criterion is that `kevinc.cpp` compiles cleanly with MSVC 2026 under
`/std:c++17 /W4`, and that representative C89 input produces syntactically
plausible KR32 assembly.

## 2. Constraints

- One translation unit, `kevinc.cpp`. No other source files.
- C++17 only. `kr32` is a read-only reference, never edited or built here.
- No third-party dependencies; the C++ standard library only.
- All work stays under `kevinc/`.
- The parser and semantic analyzer target **C89** (with a small, documented set
  of convenient extensions such as `//` comments, `long long`, and `_Bool`).
- Output is KR32 assembly text. Assembling or running it is out of scope.

## 3. Pipeline

```text
source file(s)
   │  SourceManager + Lexer (tokens, locations, line splicing, literals)
   ▼
Preprocessor (includes, object/function macros, #/##, conditionals)
   │  flat token stream
   ▼
Parser (recursive descent, C89 declarations and statements → AST)
   │  AST + parsed types
   ▼
Semantic analysis (scopes, type checking, implicit conversions, constant
   │  evaluation, frame layout)
   ▼
Code generator (KR32 assembly text, literal pool, globals, frames)
   │
   ▼
kevinc.S
```

## 4. Front end coverage

### 4.1 Preprocessor

- `#include "..."` and `#include <...>` with `-I` search paths, include-depth
  limit, and relative-to-including-file lookup.
- Object-like and function-like `#define`, `#undef`, empty arguments, `#`
  stringizing, `##` token pasting, variadic macros (`...` / `__VA_ARGS__`) as
  an extension.
- `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif` with `defined`,
  integer constant expressions, and macro expansion.
- `#line`, `#error`, `#warning` (extension), and `#pragma` (accepted; `once`
  is honored, other pragmas are ignored). `#line` does not rewrite token
  locations, so diagnostics stay tied to the physical file.
- Predefined macros: `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`,
  `__STDC__`, `__STDC_VERSION__`, `__KEVINC__`.
- `-D name[=value]` and `-U name` on the command line.
- Token-based expansion with an expansion-pass cap that stops self-recursive
  macro definitions from looping.

### 4.2 Lexer

- C89 punctuators (longest match), identifiers, pp-numbers, character
  constants, string literals, adjacent string concatenation.
- Decimal/octal/hex integers with `U`/`L` suffixes; C89 floating literals;
  escape sequences including octal and hex.
- `//` comments as an extension; `/* ... */` comments; backslash-newline
  splicing; precise file/line/column locations for diagnostics.

### 4.3 Types and declarations

- `void`, `char`, `signed`/`unsigned` variants, `short`, `int`, `long`,
  `long long` (extension), `float`, `double`, `long double`.
- Pointers, arrays (including incomplete arrays completed by an initializer),
  functions, `struct`, `union`, `enum`, and `typedef`.
- Storage classes `auto`, `register`, `static`, `extern`, `typedef`.
- Type qualifiers `const` and `volatile` accepted and tracked for diagnostics.
- C89 declarator syntax, including parentheses (`int (*fp)(int)`), array and
  function suffixes, parameter lists, prototypes, `(void)`, and K&R
  identifier-list function definitions.
- Struct/union member layout with natural alignment and padding; unions at
  offset zero; bit-fields are parsed and rejected with a clear diagnostic.
- Enum constants are compile-time integers.
- Global, local, `static` local, and `extern` variables; function prototypes
  and definitions; static and extern functions.

### 4.4 Statements

- Compound statements with declarations (C89 order enforced as a warning, not
  a hard error, to keep fixtures convenient).
- `if`/`else`, `while`, `do`/`while`, `for`, `switch`/`case`/`default`,
  `break`, `continue`, `return`, `goto`, labels, expression statements, and
  empty statements.
- Nested blocks, separate label namespace, `break`/`continue` validation.

### 4.5 Expressions

- Integer, character, string, and floating literals; identifiers; enum
  constants.
- Unary `+ - ! ~ * & ++ --`, `sizeof` (expression and type forms), casts.
- Multiplicative, additive, shift, relational, equality, bitwise, logical,
  conditional, assignment and compound-assignment, comma.
- Array subscript, `.` and `->`, direct and indirect function calls.
- C89 integer promotions and usual arithmetic conversions, with implicit
  conversions inserted as explicit cast nodes during semantic analysis.
- Constant-expression evaluation for array bounds, enum values, case labels,
  and static initializers.

### 4.6 Semantic checks

- Undeclared identifiers; redefinitions; scope and shadowing; typedef-name
  misuse; incomplete types where a complete type is required.
- Lvalue and modifiable-lvalue checks; assignment compatibility; condition
  scalar checks; call arity and argument compatibility; return-type checks;
  `break`/`continue`/`case` placement; `goto` target existence.
- Frame layout: parameters and locals at positive offsets from `R7`,
  alignment, and frame-size rounding.

## 5. KR32 code-generation mapping

The generator follows `../kr32/docs/abi.md`:

- `R0..R3` carry the first four integer/pointer arguments; `R0` returns the
  integer result. `R7` is the frame pointer. `F0` carries floating-point
  results.
- Prologue: `PUSH R7` / `ADJSP #-frame` / `LEA R7, [SP]`. Epilogue:
  `ADJSP #frame` / `POP R7` / `RET`.
- Locals and parameters are addressed as `[R7 + offset]`; expression
  temporaries use `PUSH`/`POP` below the frame.
- Scalars use `LOAD8S`/`LOAD8U`/`LOAD16S`/`LOAD16U`/`LOAD` and the matching
  stores. Floating-point values use `FLOAD.W`/`FLOAD.D` and
  `FSTORE.W`/`FSTORE.D`.
- Arithmetic maps to `ADD`, `SUB`, `MUL`, `DIVS`, `MODS`, `AND`, `OR`, `XOR`,
  `NOT`, `NEG`, `SHL`, `SHR`, `SAR`, `SLT`, `SLTU`.
- Comparisons use `SLT`/`SLTU` and branch-based sequences; `&&` and `||` are
  short-circuiting with local labels.
- Control flow uses `CMP` plus `JCOND` aliases (`JZ`, `JNZ`, `JLT`, ...),
  `JMP`, `CALL`, and `RET`. `switch` dispatches through a compare-and-branch
  chain.
- Strings and FP literals go to a `.data` literal pool and are addressed with
  `LEA`/`FLOAD`.
- Globals are emitted under `.data` with `.align`, `.global` (unless static),
  and `.byte`/`.word`/`.space`. Strings and FP literals are emitted as byte
  and word data in the same section.
- Calls evaluate up to four arguments right-to-left into 8-byte stack slots,
  then load integer/pointer arguments into `R0..R3` and scalar FP arguments
  into `F0..F3` before the call. The slot area is released with `ADJSP` after
  the call; calls with more than four arguments are rejected with a diagnostic.
- Function pointers are called through `CALL R4` after staging the target.

## 6. Deliberate C++17 surface

The file intentionally uses, in ordinary code rather than as isolated stubs:

- `std::optional`, `std::variant`, `std::string_view`, `std::filesystem`.
- `[[nodiscard]]` throughout the front end and `[[noreturn]]` on fatal
  diagnostics.
- Structured bindings (token and literal loops), `if constexpr` plus a generic
  lambda inside `std::visit` for literal formatting, fold expressions in
  `concat`, and inline `constexpr` arrays.
- `enum class`, `std::unique_ptr`, `std::shared_ptr`, `std::make_unique`.
- `std::variant` visitation with `std::visit` over integer, floating, and
  string literal alternatives.
- `std::ostringstream` for formatted diagnostics and assembly output.
- `std::array`, `std::map`, `std::set`, and `std::vector`.

## 7. Milestones

All seven milestones are delivered in `kevinc.cpp`:

1. **Skeleton** — diagnostics with caret lines, source manager, lexer.
2. **Preprocessor** — includes, object/function macros, `#`/`##`, conditionals,
   command-line defines.
3. **Parser** — declarations, declarators, statements, expressions, and
   `--dump-ast`.
4. **Semantic analysis** — scopes, conversions, constant evaluation, frame
   layout.
5. **Code generation** — globals, functions, control flow, integer expressions.
6. **Floating point and initializers** — scalar FP, aggregate initializers,
   literal pool.
7. **Hardening** — MSVC `/W4 /WX` clean, smoke corpus, `--help`, `-E`, `-S`.

## 8. Verification

- `cl /nologo /std:c++17 /permissive- /W4 /WX /EHsc /O2 /utf-8 kevinc.cpp`
  succeeds with MSVC 14.51 (Visual Studio 2026 18.10).
- `kevinc.exe --help` works.
- `kevinc.exe -E` over a macro-heavy file emits preprocessed tokens.
- `kevinc.exe -S` over the fixtures in `tests/` (scalars, control flow, structs,
  arrays, pointers, globals, functions, floats, function pointers, K&R
  definitions, preprocessor macros) writes a `.S` file.
- Every generated `.S` in `tests/` was accepted by the `kr32 as` assembler from
  the sibling `../kr32` build. The images were not executed.
- A negative test with an undeclared identifier produces a located diagnostic
  and exit status 1.

## 9. Known limits

Documented rather than hidden; each is diagnosed or a no-op where practical.

- No `_Complex`, `_Imaginary`, VLA, designated initializers, `_Generic`, or
  C99/C11 features beyond the listed extensions. `inline` and `restrict` are
  accepted and ignored.
- Bit-fields are parsed and rejected (C89 permits them; KR32 layout work is
  out of scope here).
- No full data-flow analysis, optimization, or register allocation; code is
  stack-machine shaped.
- Calls support up to four arguments total. Integer and pointer arguments use
  `R0..R3`; scalar FP arguments use `F0..F3`. Arguments beyond four, stack
  arguments, and FP stack arguments are rejected with a diagnostic.
- Aggregate arguments and aggregate returns are not implemented. Aggregate
  assignment is implemented as a byte-copy loop.
- Variadic function declarations parse, but variadic definitions and calls
  beyond the fixed four-slot staging path are not supported. `va_list` is not
  implemented.
- Floating-point increment/decrement and compound assignment are diagnosed
  rather than emitted.
- `#line` is accepted but does not rewrite token locations.
- No debug information, object format, or linker output; only `.S`.

## 10. Non-goals

- Replacing or contributing to `krc`.
- ISO C89 conformance testing or a conformance suite.
- Executing the generated programs on KR32.
- Being referenced by the repository build, tests, or documentation outside
  this folder.
