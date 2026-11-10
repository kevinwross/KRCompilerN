# Testing — KRCompiler Test Strategy

This document specifies the KRCompiler test strategy.
It is a work in progress; the authoritative specification is in `plan.md`
§5 (Corpus, conformance, and acceptance) and §15.5 (Testing).

## Status

- **Specified:** plan.md §5 (corpus tiers, sysroot, feature extraction from
  doctest strings, corpus migration checklist, conformance strategy,
  acceptance gates G0–G9) and §15.5 (unit, integration, differential,
  conformance, golden, negative, debugger, fuzzing tests).
- **Implemented:** Not yet. No test files exist in `tests/`. doctest is not
  yet vendored.

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
