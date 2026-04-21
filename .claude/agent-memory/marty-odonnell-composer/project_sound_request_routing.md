---
name: Sound request ID split
description: Nadir sound_request IDs ≥ 0x8000 route to MusicDirector; below route to SFX pool. Musical events live in the high range.
type: project
---

Nadir output buffer's `sound_request_id` field routes via a pure classifier in `src/nadir/action_sequence.cpp`:

- `[0x0000, 0x7FFF]` → `Mixer::play_sfx` (SFX pool)
- `[0x8000, 0x8FFF]` → `MusicDirector::fire_stinger`
- `[0x9000, 0x9FFF]` → `MusicDirector::set_scene_theme`
- `[0xA000, 0xAFFF]` → `MusicDirector::play_leitmotif`
- `[0xB000, 0xFFFF]` → reserved / unassigned

**Why:** keeps Nadir (a GPU compute shader) ignorant of the audio subsystem's surface — it emits an integer, the CPU routes. Having the musical vs SFX boundary encoded in the high bit (0x8000) means the branch is cheap and the intent is visible at the call site.

**How to apply:** when assigning new sound request IDs in .nadir shaders or script logic, respect the range. Reserve blocks per archetype (civilian 0x0100–0x01FF SFX + 0xA100–0xA1FF motifs, elite 0x0200–0x02FF + 0xA200–0xA2FF, etc.) so the address space stays organized.
