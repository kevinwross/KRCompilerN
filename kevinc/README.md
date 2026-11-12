# kevinc — isolated C89-to-KR32 compiler fixture

## Ownership notice (read this first)

This directory belongs to a **separate agent session** that is writing a
standalone C89 compiler as a test fixture for `KRCompilerN`'s C++17 parser.

If you are another session running in this repository:

- **Leave `kevinc/` alone.** Do not edit, move, rename, delete, format, or
  "tidy" anything under this directory. Do not add it to `CMakeLists.txt`,
  `CMakePresets.json`, CTest, CI, or any lint/format configuration.
- Treat every file here as an unrelated, opaque fixture. It is not part of the
  `krc` build and must not become one without the owner's explicit request.
- A concurrent session has uncommitted work elsewhere in the tree. That is
  expected; it is not a reason to touch this directory.
- If a change here appears necessary, stop and ask the user. Do not make it
  yourself.

The rest of the repository is unaffected by this directory. `../kr32` is a
read-only reference for the KR32 virtual CPU; nothing outside `kevinc/` is
modified by the work in this folder.

## What is here

| File | Purpose |
| --- | --- |
| `README.md` | This ownership notice. |
| `plan.md` | Scope, architecture, milestones, and known limits of the compiler. |
| `kevinc.cpp` | The entire compiler: lexer, preprocessor, C89 parser, semantic analysis, KR32 assembly code generator, and command-line driver in one C++17 translation unit. |
| `tests/` | C89 smoke fixtures and the `.S` files they produce. Not part of any build. |

`kevinc.cpp` is deliberately a single translation unit and deliberately uses a
broad slice of C++17. Its purpose is to exercise another compiler's C++ parser
(`KRCompilerN`), not to be linked into `krc`.

## Build and run

From this directory, in a Visual Studio 2026 developer environment:

```sh
cl /nologo /std:c++17 /permissive- /W4 /WX /EHsc /O2 /utf-8 kevinc.cpp /Fe:kevinc.exe
kevinc.exe -o hello.S hello.c
```

The output is KR32 assembly (`.S`). Producing or running the assembly is a
separate step; the fixture only has to compile and emit plausible assembly.

As of the timestamp below, `kevinc.cpp` builds clean with MSVC 14.51
(`/std:c++17 /permissive- /W4 /WX`) and the `.S` files in `tests/` are accepted
by the `kr32 as` assembler. They are not executed by this fixture.

## Scope

Everything here is additive and self-contained. No file outside `kevinc/` is
written by this work.

Last reviewed: 2026-11-12T11:07:08-08:00.
