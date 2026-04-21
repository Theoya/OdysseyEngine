---
name: Tempo/key grid convention
description: All music states share a common tempo grid and multiples-of-4-bars loop length so transitions are always musical.
type: project
---

House convention for music state authoring:

- **Tempo grid:** all `<music_state bpm="...">` values drawn from the set {60, 72, 80, 90, 96, 108, 120, 128, 140} so bar-aligned transitions land cleanly. Recommended baseline: exploration 80, tension 96, combat 128, victory 108.
- **Bar lengths:** `bars` attribute must be a multiple of 4 (prefer 8 or 16) so every phrase boundary is usable as a transition sync point.
- **Key relationships:** neighboring states should be a P4, P5, or relative minor/major apart. Tritone key relations only with a composed stinger bridge.
- **Modal default:** Dorian (hope/agency), Aeolian (loss), Phrygian (enemy), Lydian (awe). No plain C major unless the scene is deliberately pastoral.

**Why:** these constraints preserve the "composed transitions, not crossfades" mandate. Without a shared grid, every transition becomes a crossfade.

**How to apply:** when invoking `/music-state-create`, validate the proposed bpm and key against sibling states in the same scene. Flag any outliers (e.g. proposing 100 BPM when everything else is on the common grid).
