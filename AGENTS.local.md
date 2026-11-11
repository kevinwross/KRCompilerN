# AGENTS.local.md

Loaded immediately after `AGENTS.md`, and never overwritten by `DevTemplate` sync. This file is the repository's own layer: everything that is true of *this* project and not of every project.

Add entries here rather than editing the managed blocks in `AGENTS.md`; an edit inside a managed block is replaced on the next `--update` and will be lost.

**Keep this file current.** When a condition named here changes — a plan leaves DRAFT, a missing directory is created, a trap stops reproducing — update or delete the entry in the same change. A stale instruction is worse than a missing one: it is believed.

## Build policy

Three tiers. Run the smallest one the change needs; never run a larger one "to be safe".

**1. Inner loop — per edit and per local commit.** One build: the preset for **the operating system you are working in**, not the other one. On a Windows host that is `windows-msvc-debug`; in WSL or on Linux, `linux-gcc-debug`. **The WSL presets are the Linux row, not the Windows inner loop** — building them while working on Windows means MSVC compiles nothing until the gate, and a `/W4 /WX` failure surfaces commits later, on a change that may not be the cause. While iterating, do not configure or build a release preset, a second compiler, or another host. **A local commit is not a milestone and does not earn a matrix.**

Use the preset form: `cmake --build --preset <preset>`, then `ctest --preset <preset>`. A hand-rolled `cmake -S . -B <dir>` works only because that directory already holds the preset's cache; on a fresh one it silently produces an untyped build with whatever compiler is first on `PATH`.

**2. Boundary change — the matrix for this host.** Run the matrix when the change crosses a boundary that can break another configuration: `include/krc/`, `src/ir/`, the target descriptor, the object or symbol format, the ABI, `CMakeLists.txt`, or `CMakePresets.json` — the same categories that require an ADR. Build and test every configuration this host can reach:

| Host | Configurations |
|---|---|
| Windows | `windows-msvc-debug`, `windows-msvc-release`, plus `linux-gcc-debug`, `linux-gcc-release` in WSL |
| Linux x64 | `linux-gcc-debug`, `linux-gcc-release` |

Run only your host's row. Front-end, backend, runtime, test and documentation changes that cross none of those boundaries stay on the inner loop until the gate.

**Build the row concurrently.** Each preset has its own binary directory and they are independent, so launch them as separate background jobs and collect the results rather than running them one after another — four at once on Windows, two on Linux. Never point two presets at one build directory.

**3. Milestone gate — clean builds for this host.** Delete the build directories first; incremental output is not a gate. Build and test this host's full row. On Windows that is four configurations covering Windows x64 and Linux x64; on a Linux host, two.

**A gate subsumes the matrix.** If the gate is next — because you are about to push, or a milestone is closing — skip tier 2 and let the gate's clean builds cover it. Running the four configurations incrementally and then deleting them to run the same four again is eight builds for one answer.

**The gate closes on this host's row, not on every host.** A Windows gate passing says nothing about native Linux or ARM64, and those are expected to need fixing when first built. Do not wait for them, and do not imply they passed — record in `implementation.md` exactly which configurations were built and which remain unverified.

**Target sources rebuild with the compiler.** `krsrc/**` and `sysroot/**` are compiled by `krc`, not by the host compiler — this tree moved them out of `src/` at 746aa1b — so a green host build says nothing about them. After any change under `src/`, rebuild the target artifacts and compare: an artifact that changes when only compiler internals changed is a regression signal — investigate it before committing rather than accepting the new bytes.

## Commits and pushing

**This section authorises local commits without asking, and authorises pushing when a gate passes.**

- Commit after each coherent change, once the tier it required is green. Local commits need no permission.
- **Push when — and only when — a host gate passes.** A gate is tier 3 above: clean builds for every configuration this host can reach. Do not push on a working change, a partial matrix, or a judgement that it "looks good".
- A tier 2 change stays local until the next gate, unless the operator asks for it to be published.
- **Tidy unpublished history before pushing** if that produces a clearer record — squash fixups, reorder, reword. That rewrites only commits nobody has seen.
- **Never rewrite or force-push anything already pushed.** If CI fails on a pushed commit, fix forward with a new commit.
- One logical change per commit; subject imperative, at most 72 characters.

## Definition of done

- [ ] Builds clean with **no warnings** in **every configuration this host built**: `/W4 /WX` on MSVC, `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang (plan §15.4).
- [ ] `ctest --preset <host-debug>` is green, and so is the test run for every other configuration this host built.
- [ ] The deterministic-output check passes: the same inputs built twice compare byte-for-byte (§15.6).
- [ ] `docs/` and `implementation.md` are updated in the same change — every merged change updates the implementation log (§15.7).
- [ ] An ADR under `docs/decisions/` exists when the change touches ABI, object format, symbol format, target description, or IR.
- [ ] An Appendix A entry exists when a `D-xxx` decision changes.
- [ ] No external runtime dependency added; doctest is the only vendored test dependency.
- [ ] No secrets, debug output, or unrelated edits in the diff.

## Platform targets

<!-- Generated by `sync.mjs --targets=...`; regenerated on `--update`. The Notes column is
     yours and is preserved. The universal portability rules in AGENTS.md apply whether or
     not anything is listed here. -->
<!-- project:targets:start — generated from .dev-template.json targets; edit the Notes column, not the rows -->
| Target | Architecture | Verified by | Notes (yours — preserved on update) |
|---|---|---|---|
| Windows | x64 | <windows-latest in CI, or name the dev machines> | MSVC 19.51 Debug/Release verified locally 2026-11-09; 19/19 CTest; remote CI pending. |
| Linux | x64 | <ubuntu-latest in CI> | WSL Alpine 3.22 GCC 14.2 Debug/Release and Clang 20.1 Debug/ASan verified locally 2026-11-09; 19/19 CTest; remote CI pending. |

Target-specific notes:

- **Windows:**
  - Long paths beyond 260 characters fail unless long-path support is enabled.
  - Use the language path API even though Windows accepts `/`; the tool that rejects a slash is usually native, not the runtime.
- **Linux:**
  - Filename case is significant here. An import that mismatches on-disk case resolves on Windows and fails here.
<!-- project:targets:end -->

**Host, not VM.** This table is where the toolchain is *built and tested*. The compiler's output targets are KR32 and KR16 — they are not host platforms and do not belong here. ARM64 host CI is deliberately deferred (§15.4); the compiler must nevertheless stay portable C++20, so do not introduce host-specific code without an ADR.

## Architecture boundaries

Dependencies flow one way: **front end → IR → backend → assembler → linker**, with shared services (diagnostics, source manager, target descriptors) available to all and depending on none of them.

- `src/frontend/` — preprocessor, lexer, parser, Sema. Must not depend on any backend, linker, or runtime code.
- `src/ir/` — typed SSA and IR validation. Must not depend on a specific target; target-specific choices belong in `src/backend/` and the target descriptor.
- `src/backend/` — instruction selection, register allocation, frames, asm printing, per-target descriptors. Depends on IR, never the reverse.
- `src/asm/`, `src/linker/`, `src/debug/` — object format, `krld`, symbol/debug format. The linker must not reach back into the front end.
- `krsrc/` — runtime and libc support for the VMs, compiled by `krc` for the VM targets. **Never built by the host compiler, and never added to the host CMake build**: a source here that the host build tries to compile is a wiring error, not a missing dependency. Must not depend on compiler internals.
- `include/krc/` — the public surface. Keep it minimal; internal headers stay under `src/`.
- One compiler driver: `krc` orchestrates; `krld` remains a standalone binary. Do not add a third entry point.

## Environment assumptions

- **Doctor:** `pwsh -File .agents/doctor.msvc.ps1` reports whether this machine can build with MSVC, and exits 2 naming each component that is missing. Run it before a host gate, and whenever a build cannot find its compiler. No doctor exists for WSL yet; there, configuring `linux-gcc-debug` is the check. Resolved toolchains and their verification dates live in the Platform targets table above — do not restate them here.
- CMake 3.25 or newer; C++20; no external runtime dependencies.
- MSVC on Windows and GCC/Clang on Linux, both with warnings as errors. Sanitizers (ASan/UBSan) are configured on Linux only.
- doctest is vendored for tests — do not replace it with a fetched framework.
- Sibling repositories `../kr32` and `../kr16` are **read-only inputs** (their docs, corpus and conventions are cited throughout `plan.md`). Never edit them from here.

## Known traps

- **This began as a working copy of `../KRCompiler`, and the two have diverged.** Both `plan.md` files are approved v0.1, but they are no longer the same document: this tree moved runtime sources to `krsrc/` at 746aa1b, and the original carries later revisions. Edits here do not propagate to it. Before treating a `plan.md` rule as shared, compare the two copies; when you change this one, say so in your response so they can be reconciled deliberately rather than drifting.
- **`AGENTS.local.md` is an instruction file, not a log.** Update it when a rule it states becomes false; never record progress, status, or milestone state here — that belongs in `implementation.md`. Every edit changes how future work is done, so keep edits rare and reviewable.
- **The plan is approved (2026-11-09).** Implement under §18.2; `implementation.md` is the live tracker. A `D-xxx` change needs an Appendix A entry; ABI, object, symbol, target, or IR changes also need an ADR under `docs/decisions/`.
- **"Target" is two words here.** Host platforms (Windows, Linux) versus VM targets (KR32, KR16). Confusing them produces wrong CI matrices, wrong ABI assumptions, and target-descriptor code checked into the front end.
- **Warnings-as-errors cuts both ways.** A change verified only on MSVC can fail the Linux build on `-Wpedantic`, and vice versa. When a warning appears, fix the code; do not add a pragma, and do not narrow the warning set.
- **Determinism is byte-exact.** Sorted symbols, stable section order, no timestamps, build-root remapping (§15.6). A new field in an object, symbol table, or IR dump can silently break the double-build comparison — run it, do not assume it.
- **Every merged change updates `implementation.md` and the owning doc.** The plan names it as the live tracker; a change without the log update is incomplete.
- **ABI, object, symbol, target and IR changes need an ADR first.** Not a note in the commit message — a file under `docs/decisions/`.
- **`D-xxx` identifiers are stable.** A changed decision takes an Appendix A entry; renumbering or deleting a decision breaks every reference that cites it.
- **The compiler is one program with many outputs** (`-c` object, `-S` assembly, `-o` linked image). Do not grow a second driver to handle a case; extend `krc`.
- **CI runs the full matrix.** A golden or differential test that passes locally on one host proves nothing about the other; report which toolchain you ran.
- **Timestamps are read, not remembered.** Call `local-datetime` immediately before writing any timestamp, and copy its `iso` field verbatim — `2026-11-09T21:57:03-08:00`. Do not compose one from `year`/`month`/`day` (they are not zero-padded), and do not put `datetime` or `date` in a record: `11/9/2026` is locale-ordered and ambiguous. A timestamp inferred from the conversation or from a build artefact is stale by however long the work has run.

