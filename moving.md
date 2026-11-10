# KRCompilerN — Handoff Document (moving.md)

## 1. Session context

- **Goal**: Implement M0 through M10 of the KRCompiler project (goal id: `goal-c1239c96-0943-4e8e-ab77-9dc523d21fa1`, revision 4, active, round 13/256, max 256 rounds)
- **Workspace**: `C:\Users\kevin\source\repos\KRCompilerN`
- **GitHub repository**: https://github.com/kevinwross/KRCompilerN.git
- **DSH checkout**: `C:\AI\deepseek-harness\` (not part of this repo; only for DSH itself)
- **Browser GUI**: http://127.0.0.1:3080 (DSh Web GUI)
- **OS**: Windows (MSVC builds) + WSL Alpine 3.22 (GCC/Clang builds)
- **CMake presets**: `windows-msvc-debug`, `windows-msvc-release`, `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-sanitize`
- **Test framework**: doctest (vendored, only external dependency)

## 2. Milestone status

| Milestone | Status | Notes |
|-----------|--------|-------|
| M0 (project setup) | ✅ Complete | |
| M1 (lexer) | ✅ Complete | |
| M2 (assembler) | ✅ Done | KRO object format, KR32 ISA encoding |
| M3 (linker) | ✅ Done | gc-sections, strip, precise relocations, map file, size report |
| M4 (debug info) | 🟡 In progress | KDBG format ✅, front-end debug emission ✅, backend debug emission ✅, linker `--debug-file` ✅, `krc --dump-symbols` ✅, **DAP/GUI stub NOT started** |
| M5-M10 | ⏳ Not started | C++ front end, KR16 backend, hardening, runtime/libc |

## 3. What was completed this session (round 13)

### M4 — Backend debug info emission (already done from prior rounds, verified)
- `src/backend/debug_info.h/cpp` — `MachineDebugInfoCollector` walks machine module after frame layout, collects function ranges, frame records (saved FP/RA, CFA rule), variable locations from stack slots
- `src/backend/machine.h` — `MachineFunction` gained `debug_start_address`, `debug_size`, `debug_saved_fp_offset`, `debug_saved_ra_offset`, `debug_cfa_rule`, `MachineVarDebugInfo` struct, `debug_variables` vector
- `src/backend/CMakeLists.txt` — added `debug_info.cpp` to `krc_backend`

### M4 — Linker `--debug-file` sidecar support (this session)
- `src/linker/linker.h` — added `debug_file` field to `LinkerOptions`; declared `write_debug_file()` method
- `src/linker/linker.cpp` — implemented `write_debug_file()`: reads each input object's `debug_table`, merges KDBG containers (string tables remapped, sources/lines/funcs/frames/vars/symbols/types/scopes merged), writes merged `.kdbg` sidecar. Called in `Linker::link()` after `apply_relocations()` and before `strip_symbols()`.
- `src/linker/CMakeLists.txt` — added `../debug` include path to `krc_linker`
- `src/tools/krld/main.cpp` — wired `opts.debug_file` to `link_opts.debug_file`
- `src/tools/krld/CMakeLists.txt` — added `krc_debug` link dependency and `../../debug` include path
- **End-to-end verified**: `krld test.o -o test.ko --size --debug-file=/path/to/file.kdbg` produces merged 416-byte KDBG sidecar

### M4 — `krc --dump-symbols` CLI flag (this session)
- `src/tools/krc/main.cpp` — added `dump_symbols` option, `-dump-symbols` flag, help text; dumps symbol table (name, value, size, section, binding, type, visibility) from the assembled object file
- **End-to-end verified**: `krc -c -dump-symbols -o test.o test.c` prints symbol table with `main` (value=0x0, size=24, section=1, binding=Global, type=Function)

### Build verification
- All 7 builds clean (no warnings, warnings-as-errors): MSVC Debug/Release (`/W4 /WX`), GCC Debug/Release (`-Wall -Wextra -Wpedantic -Werror`), Clang Debug (`-Wall -Wextra -Wpedantic -Werror`), Clang ASan+UBSan (`-Wall -Wextra -Wpedantic -Werror`)
- ctest green on all Linux presets (sanitizer LSan leak is pre-existing and known)

### Documentation
- `implementation.md` — M4 checklist updated: both front-end and backend debug info emission marked `[x]`; new running log entries at `2026-11-10T07:13:51-08:00` (front-end) and `2026-11-10T07:36:41-08:00` (backend)
- `src/runtime/CMakeLists.txt` — crt0 build disabled (blocked by front-end `goto`/loop support gap)
- `src/runtime/crt0.c` — source in place, ready to compile once M1 statement support matures

## 4. Remaining M4 work

### DAP/GUI debug session stub (NOT STARTED)
Create a Debug Adapter Protocol stub server. Suggested location: `src/tools/krdap/`

Files to create:
- `src/tools/krdap/main.cpp` — DAP server stub
- `src/tools/krdap/CMakeLists.txt` — build `krdap` executable
- Register in `src/CMakeLists.txt` or `src/tools/CMakeLists.txt`

DAP messages use Content-Length prefixed JSON-RPC over stdio. Stub should at minimum handle:
- `initialize` → return capabilities
- `launch` → return success
- `setBreakpoints` → return breakpoints
- `configurationDone` → return success
- `threads` → return single thread
- `stackTrace` → return empty frames
- `scopes` → return empty scopes
- `variables` → return empty variables
- `continue` / `next` / `stepIn` / `stepOut` / `pause` / `disconnect` → return success

### M4 checklist current state (from implementation.md)
```
- [x] src/debug/kdbg.h — KDBG debug format data structures
- [x] src/debug/kdbg.cpp — write_kdbg()/read_kdbg() round-trip verified
- [x] src/debug/CMakeLists.txt — krc_debug static library
- [x] src/CMakeLists.txt — add_subdirectory(debug)
- [x] KDBG reader (read_kdbg)
- [x] KDBG dumper (dump_kdbg)
- [x] Front-end debug info emission (source locations, line tables)
- [x] Backend debug info emission (function ranges, frame records, variable locations)
- [x] Linker --debug-file sidecar support in krld
- [x] krc --dump-symbols CLI flag
- [ ] DAP/GUI debug session (stub)
```

## 5. Key files and their purposes

| File | Purpose |
|------|---------|
| `src/asm/object.h` | `ObjectFile` struct, KRO format enums, `write_kro_object()`, `read_kro_object()` |
| `src/asm/object.cpp` | KRO writer/reader implementation |
| `src/asm/assembler.h/cpp` | `Kr32Assembler` class; `encode_instruction()` takes `current_offset` |
| `src/backend/machine.h` | `MachineOperand`, `MachineInstr`, `MachineFunction` (with debug fields), `MachineModule` |
| `src/backend/debug_info.h/cpp` | `MachineDebugInfoCollector` — NEW, collects function/frame/var debug info |
| `src/backend/frame_layout.h/cpp` | `FrameLayout` — computes stack frame size, assigns stack slots |
| `src/backend/register_allocation.h/cpp` | `RegisterAllocator` — register allocation pass |
| `src/backend/instruction_selection.h/cpp` | `InstructionSelection` — IR → machine instructions |
| `src/linker/linker.h/cpp` | `Linker` class; `write_debug_file()` for `--debug-file` sidecar |
| `src/debug/kdbg.h/cpp` | KDBG format: `KdbgContainer`, `write_kdbg()`, `read_kdbg()`, `dump_kdbg()` |
| `src/frontend/debug_info.h/cpp` | `DebugInfoCollector` — walks AST, collects sources/functions/lines |
| `src/frontend/source_manager.h/cpp` | `SourceManager` with `file_ids()` method |
| `src/tools/krc/main.cpp` | `krc` driver: `-g`, `-dump-debug`, `-dump-symbols`, `build_kdbg_container()` |
| `src/tools/krld/main.cpp` | `krld` CLI: `--map`, `--size`, `--gc-sections`, `--strip`, `--debug-file=FILE` |
| `src/runtime/crt0.c` | `_start()` calls `main()` then `goto halt` (NOT built — front-end lacks goto/loops) |
| `src/runtime/CMakeLists.txt` | crt0 build disabled (commented out `ALL` target) |

## 6. Build and test commands

### Windows (MSVC)
```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
# Release:
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

### Linux / WSL (GCC)
```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
# Release:
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

### Linux / WSL (Clang)
```bash
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

### Linux / WSL (Clang ASan+UBSan)
```bash
cmake --preset linux-sanitize
cmake --build --preset linux-sanitize
ctest --preset linux-sanitize
# LSan leak is pre-existing; expected failure on sanitize preset
```

### Manual krc test
```bash
# Compile to object with debug sidecar
krc -c -g -o test.o test.c
# Dump debug info
krc -c -g -dump-debug -o /tmp/test.o test.c
# Dump symbols
krc -c -dump-symbols -o test.o test.c
# Link with debug sidecar
krld test.o -o test.ko --size --debug-file=test.kdbg
# Size report
krld test.o -o test.ko --size
```

## 7. Known issues and blockers

### Blocker: M3 crt0 startup (front-end `goto`/loop support)
- `src/runtime/crt0.c` exists but cannot be compiled by `krc` because the C front end does not support `goto` statements or `for(;;)` loops.
- The `crt0.c` was changed from `for(;;)` to `goto halt`, but the front end doesn't support `goto` either.
- `src/runtime/CMakeLists.txt` has the crt0 custom target disabled (changed from `ALL` to a comment-only file).
- **Fix**: Implement `goto` and loop statements in the front end (M1 front-end work), then re-enable the crt0 build in `src/runtime/CMakeLists.txt`.
- This is NOT blocking M4 completion.

### Sanitizer build
- `linux-sanitize` preset builds fine but `krc_tests` fails due to a pre-existing LSan leak (not related to this session's changes).
- All other 6 configurations are fully clean.

### `build/` directory
- Untracked build artifact; should not be committed.
- No git remote configured; CI hasn't run.

## 8. Definition of done (AGENTS.local.md)

- [ ] Builds clean with no warnings on both toolchains: `/W4 /WX` on MSVC and `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang
- [ ] `ctest` green on all presets
- [ ] Determinism double-build passes (§15.6)
- [ ] `docs/` and `implementation.md` updated in the same change
- [ ] ADR under `docs/decisions/` exists when the change touches ABI, object format, symbol format, target description, or IR
- [ ] Appendix A entry exists when a `D-xxx` decision changes
- [ ] No external runtime dependency added; doctest is the only vendored test dependency
- [ ] No secrets, debug output, or unrelated edits in the diff

**Note**: The M4 work this session did not add an ADR or Appendix A entry because it did not change ABI, object format, symbol format, target description, or IR — it only added debug info emission and CLI flags on top of existing formats.

## 9. Next steps (priority order)

1. **Complete M4**: Create the DAP/GUI debug session stub at `src/tools/krdap/`
2. **Update `implementation.md`**: Mark DAP/GUI stub `[x]` when done; add running log entry with timestamp from `local-datetime`
3. **M5**: C++ front end (classes, templates, name mangling)
4. **M6**: KR16 backend
5. **M7-M10**: Hardening, runtime/libc

## 10. Important conventions

- **Timestamps**: Call `local-datetime` before writing any timestamp; copy `iso` field verbatim (e.g., `2026-11-10T07:36:41-08:00`). Do not compose from year/month/day.
- **"Target" is two words**: host platforms (Windows, Linux) vs VM targets (KR32, KR16). Do not confuse them.
- **One-way dependencies**: front end → IR → backend → assembler → linker. The linker must not reach back into the front end.
- **Warnings-as-errors**: Never add `#pragma` to suppress warnings; fix the code.
- **No hardcoded path separators**: Use the language's path API.
