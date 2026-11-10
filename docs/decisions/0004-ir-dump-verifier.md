# ADR 0004 — IR textual dump and verifier

**Status:** proposed  
**Date:** 2026-11-09  
**Decision:** D-005, plan.md §8.9, §8.10

## Context

The IR needs a human-readable textual form for debugging and teaching, and a
verifier to catch IR construction bugs before codegen (plan.md §8.9, §8.10).

## Decision

- `krc --dump-ir` prints a stable-for-humans (not stable-for-parsing) form.
  Dumps are for debugging and teaching; they are not a compatibility surface.
- The verifier checks SSA dominance and single assignment, type consistency of
  operands and results, terminator presence and block references, phi-node
  predecessors, exception edge/landingpad pairing, alignment and volatility
  flags, and target-profile legality.
- A failed verification is a compiler bug and MUST abort with a diagnostic,
  not emit code (plan.md §8.10).

## Consequences

- Positive: readable IR/assembly dumps support teaching and debugging (G-8);
  the verifier prevents miscompilation from malformed IR.
- Negative: textual dumps are not a compatibility surface, so they may change
  without notice.

## Related

- `docs/ir.md` — IR specification
- `src/ir/` — IR model, builder, verifier, printer, passes (not yet created)
- `plan.md` §8.9, §8.10
