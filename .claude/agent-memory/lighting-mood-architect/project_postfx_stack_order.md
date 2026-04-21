---
name: Post-FX chain order is fixed; CRT + EVA HUD are engine-owned
description: Chain is tonemap → bloom → grade → vignette → grain → CRT → EVA HUD; profiles only parameterize the first five.
type: project
---

The OdysseyEngine post-FX chain order is engine-enforced and non-negotiable: `tonemap → bloom → grade → vignette → grain → CRT → EVA HUD → swapchain`. Lighting profile XMLs author parameters for the first five passes only. CRT and EVA HUD are owned by the engine and must never be parameterised or reordered from a profile.

**Why:** Charter Pillar 3 says post-FX is tonal, not cosmetic. CRT + EVA are the project's tonal signature (Evangelion-lineage HUD) — per-profile overrides would let individual scenes drift from that signature. Also, reordering would invalidate the `/barrier-audit` expected-edge graph.

**How to apply:** When writing a lighting profile, never include a `<crt>` or `<eva>` block. When proposing a new post-FX pass, it must go through `/postfx-add` with explicit chain-position, and it cannot land after the CRT stage. If a scene asks for "different CRT settings," that's a charter discussion, not a profile change.
