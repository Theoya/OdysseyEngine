---
name: Flicker must be deterministic (tick-index + seed, not wall clock)
description: Flicker curves in this project must be seeded value-noise keyed on tick index, not delta-time or wall-clock, so replay hashes stay stable.
type: feedback
---

Flicker implementations in this project must compute `amp * noise1D(freq * tick_index + seed_hash(seed))` where `tick_index` is the integer simulation tick, not a wall-clock time. Random()/rand()/hash-of-elapsed-seconds are all wrong.

**Why:** Netcode's replay determinism story depends on byte-identical LightBuffer rows across sessions. A flicker keyed on wall-clock diverges on the first frame pacing jitter and poisons every replay hash downstream. `/replay-play` will surface the first divergent tick and blame lighting.

**How to apply:** When authoring or reviewing any flickering light, verify the curve source is `tick_index`, not `time_seconds` or `delta`. Offer seed values explicitly in XML (e.g. `seed="0xA17C4"`). Hash the LightBuffer row across two runs to confirm.
