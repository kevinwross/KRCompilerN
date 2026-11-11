# ADR Index — Architecture Decision Records

This directory holds Architecture Decision Records (ADRs) for changes that
touch ABI, object format, symbol format, target description, or IR
(plan.md §18.1). A `D-xxx` decision change also requires an Appendix A entry
in `plan.md`.

ADRs are numbered sequentially. Changing a `D-xxx` decision requires editing
its row in `plan.md` Appendix A and adding an ADR here.

## ADRs

| ADR | Title | Status | plan.md decision |
|---|---|---|---|
| 0001 | C/C++ ABI (KR32 base, versioned) | proposed | D-010, D-011 |
| 0002 | Target descriptor schema v1 | proposed | D-007, §4.2 |
| 0003 | IR core — typed SSA, memory model | proposed | D-005, D-006, §8 |
| 0004 | IR textual dump and verifier | proposed | D-005, §8.9, §8.10 |
| 0005 | KR16 relocation types | proposed | D-013, D-014, §11.4 |

## Process

- An ADR is required before any change to ABI, object format, symbol format,
  target description, or IR.
- The ADR must describe the decision, its rationale, consequences, and any
  migration notes.
- `D-xxx` identifiers are stable; renumbering or deleting a decision breaks
  every reference that cites it (plan.md known traps).

## Related

- `plan.md` Appendix A — full decision log (D-001 through D-035)
- `plan.md` §18.2 — after-approval steps
