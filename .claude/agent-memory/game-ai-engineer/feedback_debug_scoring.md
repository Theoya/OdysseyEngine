---
name: Expose per-consideration scores via debug_pack4
description: /inspect-scoring can only show winners unless the .nadir explicitly packs raw scores into the debug SSBO
type: feedback
---

When authoring a `.nadir`, always end `main()` with an explicit debug-lane
write of the per-consideration scores, not just the winning-state color.

**Why:** the debug SSBO is a single `vec4` per entity. `debug_state_color`
(in `behaviors/lib/debug.glsl`) consumes all four lanes to encode the winner.
Without additional lanes exposed, `/inspect-scoring` can only report the
winning state — not the scores that lost, which is exactly what the tuner
needs to understand "why did flee win here?"

**How to apply:** either pack the four key scores:

```glsl
debug_pack4(idx, score_combat, score_flee, score_patrol, score_alert);
```

… or alternate per-frame (odd frames = state color, even frames = scores)
using `frame_number & 1u`. The `/inspect-scoring` doc advises the caller
to add this when scores aren't visible; keeping it on by default in every
non-trivial `.nadir` saves that back-and-forth.
