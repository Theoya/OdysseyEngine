---
name: Audio subsystem direction
description: First-principles mixer + 4-API MusicDirector adopted after Marty's rejection forced council escalation; no miniaudio, no single-float intensity.
type: project
---

Audio architecture decision: hand-authored mixer and MusicDirector in `src/audio/`, Windows-only WASAPI backend, no third-party audio middleware.

**Why:** council vote 2026-04-20 hit 77.8% (below 80% consensus); Marty (weight 4) rejected the miniaudio + single-intensity-float proposal. User sided with Marty on escalation. The intent is that every DSP primitive is derivable from first principles and understood by the team — preserving the purity/lean mandate and the "everything understood" mandate.

**How to apply:** when proposing audio changes, do NOT suggest importing miniaudio, FMOD, Wwise, or any middleware. Do NOT collapse the four MusicDirector APIs (set_scene_theme, set_intensity_layers, fire_stinger, play_leitmotif) into a single parameter or RTPC. Each is orthogonal by design.
