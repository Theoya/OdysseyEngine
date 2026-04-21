---
name: Showcase lighting zone map
description: Six-zone spatial lighting plan inside the single showcase arena, one LightingProfile per zone.
type: project
---

The showcase scene (`demo/showcase/showcase.scene.xml`) is a single 100x100 arena carved into six spatial zones, one per canonical profile (Sacred, Wonder, Dread, Liminal, Hostile, Warmth). Each zone has its own LightingProfile XML under `demo/showcase/lighting_profiles/`. The `sun` directional light is reused across zones with per-zone direction/kelvin/intensity overrides rather than six separate directionals.

**Why:** The showcase is the canonical acceptance arena for every subsystem; doing six zones in one scene lets a single traversal cover all six moods for charter/vibe audit without scene switching. Also enforces the 8-light LightBuffer budget as a hard ceiling because only one zone is active at a time.

**How to apply:** When future conversations reference "the showcase lighting" or add lights there, honor the zone AABBs in `design/lighting.md` §1 and keep zone-local light count such that `sun + flashlight + player_aura + zone_local_lights ≤ 8`. South Pit is at cap (8) with the 4-sample virtual area light — do not add more without replacing the scheme.
