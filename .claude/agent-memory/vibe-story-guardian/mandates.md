---
name: Engineering mandates (four)
description: The four non-negotiable engineering mandates that every felt-experience and code proposal must respect
type: project
---

Four standing mandates persisted in project memory and CLAUDE.md; they gate every line of code and every charter proposal.

**Why:** The user's "everything understood" commitment is both an engineering principle and a charter pillar. Mandate #4 (no black boxes) is load-bearing — it is why we rejected Jolt, miniaudio, and cargo-culted PBR at the 2026-04-20 council, and is directly encoded as Pillar 4 in the charter.

**How to apply:**
1. Pure/lean functions by default — side effects isolated to thin I/O wrappers.
2. Success + failure tests for every Result<T,E>-returning function.
3. First-principles math — every formula derivable from a comment pointing to its derivation.
4. Everything understood — no third-party library we can't explain line-by-line.

When evaluating a proposal, if it imports a library whose behavior/rendering semantics we can't read and modify, it hits Forbidden entry #3 and is rejected on charter grounds, not just engineering grounds.
