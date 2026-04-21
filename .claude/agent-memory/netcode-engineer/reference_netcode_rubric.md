---
name: Netcode rubric location
description: Canonical netcode design rubric — consult before every protocol, replication, or latency-masking decision, and before any council vote involving §9 triggers.
type: reference
---

Rubric lives at `C:\Users\THadfield\.claude\agent-knowledge\netcode-engineer\netcode_rubric.md`.

Ten sections: (1) first-principles UDP protocol (sequence+ack+ack_bits, Jacobson/Karels retry, MTU), (2) snapshot interp vs rollback vs lockstep and why OdysseyEngine picks snapshot interp (Nadir GPU non-determinism), (3) lag compensation derivation and anti-cheat sanity checks, (4) grid-based interest management + priority queue fallback, (5) fixed-point authority path for deterministic replay, (6) security boundaries (server-authoritative list, scripts-never-replicated, signed manifest), (7) bitpacking math (u16 position, smallest-three 9-bit quat, u8 health → 17 B per entity snapshot), (8) debuggability baselines (what "normal"/"desync"/"cheat" look like in the Network Panel), (9) council triggers owned by this agent, (10) verified reference URLs.

When to cite: every PR description touching `src/net/*` should reference the relevant section. Every council vote on a netcode topic should paste the section and note any deviation.

The four council triggers this agent owns are specified in §9; the standing REJECT triggers are also there (third-party netcode libs, replicated runtime scripts, client-side hit reg, disabling PROTOCOL_ID magic).
