# Showcase — Lighting contribution

**Agent:** lighting-mood-architect
**Scope:** light placement, Kelvin palette discipline, flicker determinism, fog/volumetrics per zone, post-FX stack verification, bindless LightBuffer coverage.
**Root:** `demo/showcase/`
**Charter binding:** `docs/vibe_charter.md` v0 — Pillar 3 ("legibility over spectacle") and Pillar 6 ("silence carries the register") apply directly. Post-FX is tonal, not cosmetic. Darkness is a feature; the player reads composition, not illumination.

---

## 1. Zone plan

The showcase arena is a 100 m x 100 m bounded box (walls at ±50). Instead of six separate levels, we carve **six spatial zones** inside the single arena, each tagged with a distinct lighting profile. The player traverses all six on a single loop. Transitions are gated by `LightingSystem::blend_profiles(from, to, t)` driven by the player's X/Z position against zone AABBs. One zone is *active* per frame; its `<lighting_profile>` is what the `<scene profile="…">` attribute resolves to for acceptance runs.

| Zone        | AABB (x_min z_min x_max z_max) | Profile   | Primary mood                              | Dominant source              |
|-------------|--------------------------------|-----------|-------------------------------------------|------------------------------|
| North Altar | -15 30  15 50                  | `Sacred`  | Reverence, hero-scale, warm/cool split    | Directional sun + altar candles |
| West Chapel | -50 -10 -15 30                 | `Wonder`  | Awe, god rays through pillars             | Directional key + volumetric beams |
| South Pit   | -15 -50  15 -10                | `Dread`   | Oppression, low-key, cool shadows         | Boss-emissive + dying embers |
| East Lab    |  15 -10  50 30                 | `Liminal` | Uncanny flatness, fluorescent hum         | Flat fluorescent fill        |
| NE Courtyard|  15  30  50 50                 | `Hostile` | Alarm, hard shadows, pulsing red          | Pulsing point + harsh rim    |
| SW Hearth   | -50 -50 -15 -10                | `Warmth`  | Safety, tungsten hearth                   | Fire pit + lantern chain     |

Each zone's LightingProfile XML is under `demo/showcase/lighting_profiles/`. The master scene's current `lighting_profile="liminal"` is the **startup zone** (player spawns at 0 1 25 — but the `mood-apply` script picks the zone under the player's feet each frame; liminal is just the fallback).

---

## 2. Light inventory

All lights live in `showcase.scene.xml` as `<entity archetype="light">` nodes, registered to the LightBuffer SSBO via `/add-light`. Total dynamic on-screen at any moment: **at most 8** (one zone active). Full scene roster: **14 lights** (6 zone-local + 3 shared + 5 already stubbed).

### Already stubbed in scene (extend, do not duplicate)

| ID             | Type        | Kelvin | Intensity | Notes                                    |
|----------------|-------------|-------:|----------:|------------------------------------------|
| `sun`          | directional | 5500   | 3.0       | Global key; **direction rotates per zone** — see §3 |
| `torch_north`  | point       | 1900   | 8.0       | North Altar candle-secondary; flicker seeded |
| `flashlight`   | spot        | 4200   | 12.0      | Player-attached; follows camera (§5)     |

### New lights added by this contribution

| ID                   | Type   | Zone        | Kelvin | Intensity | Range | Flicker | Notes                                   |
|----------------------|--------|-------------|-------:|----------:|------:|:-------:|-----------------------------------------|
| `altar_candle_L`     | point  | North Altar | 1850   | 6.0       | 8     | yes     | Candle flicker, seeded 0xA17A8          |
| `altar_candle_R`     | point  | North Altar | 1850   | 6.0       | 8     | yes     | Mirror of L, seeded 0xA17A9 (different phase) |
| `chapel_godray_key`  | spot   | West Chapel | 6200   | 14.0      | 40    | no      | Overhead-through-window; drives volumetric beam |
| `pit_boss_emissive`  | point  | South Pit   | 10000  | 5.0       | 18    | yes     | Boss-gunner aura; 2 Hz stutter, seeded 0xB055 |
| `pit_ember_area`     | area   | South Pit   | 1500   | 2.5       | 25    | yes     | Low-intensity floor "area" light via 4-sample virtual panel (§4) |
| `lab_fluorescent`    | spot   | East Lab    | 4000   | 10.0      | 30    | yes     | Cool-white ceiling; 50 Hz hum flicker, amp 0.04 |
| `courtyard_pulse`    | point  | NE Courtyard| 2200   | 9.0       | 20    | yes     | Sodium-orange alarm, 1.2 Hz square pulse |
| `hearth_firepit`     | point  | SW Hearth   | 1850   | 7.0       | 14    | yes     | Fireplace, seeded 0xF17E0 (asymmetric noise) |
| `hearth_lantern_1`   | point  | SW Hearth   | 2700   | 4.0       | 10    | no      | Tungsten anchor, steady                  |
| `hearth_lantern_2`   | point  | SW Hearth   | 2700   | 4.0       | 10    | no      | Tungsten anchor, steady                  |
| `player_aura`        | point  | **follows** | 3200   | 2.0       | 6     | no      | Low-intensity halogen, player-attached (§5) |

**On-screen budget verification:** each zone's active subset = {sun, flashlight, player_aura} + zone-local lights ≤ 5. Peak zone is SW Hearth: sun + flashlight + player_aura + firepit + lantern_1 + lantern_2 = **6 lights**. Well under the 8-light LightBuffer budget.

### Area light note
Odyssey doesn't ship a true area-light primitive yet. `pit_ember_area` is a **4-sample virtual panel**: four jittered point lights at Kelvin 1500, each at intensity 0.625, arranged on a 2x2 grid 5 m below the pit rim. This costs 4 LightBuffer slots but reads as a glowing floor. When a real area primitive lands (council vote required — new render pipeline feature), collapse these to one node.

---

## 3. Directional sun rotation per zone

The `sun` entity's `direction` attribute is **rewritten per zone** at profile-apply time so the same directional light serves six moods:

| Zone        | Direction (x y z, normalised) | Effective angle | Rationale                     |
|-------------|-------------------------------|-----------------|-------------------------------|
| North Altar | `-0.0 -0.9 -0.4`              | 25° off vertical| Sacred key from above          |
| West Chapel | `-0.5 -0.4 -0.3`              | shallow east    | God-ray slant through pillars  |
| South Pit   | `0 -1 0` (essentially off)    | overhead, dim   | Sun muted to 0.4 intensity — Dread reads from below |
| East Lab    | `0 -1 0` at intensity 0.3     | neutral         | Liminal disowns directionality; flat dominates |
| NE Courtyard| `0.6 -0.3 -0.5`               | harsh low sun   | Hostile — long shadows, warm angry |
| SW Hearth   | `-0.3 -0.8 -0.2` at int 1.2   | reduced overhead| Warmth — hearth dominates, sun backs off |

The sun's **Kelvin** also migrates per zone: 5500 (neutral), bumped to 6200 for Wonder/Sacred, down to 3200 for Warmth, up to 7500 for Liminal's sky-fill. The engine handles this via `LightingProfile::directional_override` applied by `LightingSystem::apply_profile`.

---

## 4. Flicker determinism

All flickering lights use the engine's **seeded flicker curve**: `flicker(t, seed) = amp * noise1D(freq * t + seed_hash(seed))` where `noise1D` is a value-noise function over a fixed lattice (not runtime random). Determinism property: same `(t, seed)` → same value across sessions, platforms, and replay playbacks. This is what `/replay-play` depends on.

Authoring via `/light-flicker-tune <light_id> --amplitude=A --frequency=F --seed=S`:

| Light               | Amplitude | Frequency (Hz) | Seed     | Character                        |
|---------------------|----------:|---------------:|---------:|----------------------------------|
| `torch_north`       | 0.15      | 6              | `0xA17C4`| Gentle breath                    |
| `altar_candle_L`    | 0.18      | 7              | `0xA17A8`| Candle — slightly faster, softer |
| `altar_candle_R`    | 0.18      | 7              | `0xA17A9`| Mirror, different phase          |
| `pit_boss_emissive` | 0.35      | 2              | `0xB055` | Stuttering malevolent pulse      |
| `pit_ember_area` ×4 | 0.10      | 3              | `0xE17E0..3` | Low, asymmetric — embers     |
| `lab_fluorescent`   | 0.04      | 50             | `0xF100E`| 50 Hz mains hum                  |
| `courtyard_pulse`   | 0.50      | 1.2            | `0xA1A82`| Hard square-ish pulse (clamp)    |
| `hearth_firepit`    | 0.22      | 4              | `0xF17E0`| Asymmetric fire noise            |

**Showcase target:** `torch_north` is the flagship flicker. Running `/light-flicker-tune torch_north --amplitude=0.15 --frequency=6 --seed=0xA17C4` twice on the same frame index must produce bit-identical light values. That is the acceptance contract and what `/replay-play` hashes.

---

## 5. Dynamic player-following light

`player_aura` is attached via a one-line script in `showcase_player.cpp` that copies the player's transform into the light's `position` uniform each frame (with a +0.3 m Y offset so it doesn't clip the floor). The **spotlight** `flashlight` is attached the same way, but its direction tracks the camera's forward vector. These two lights are the only ones that move per-frame; all others are static transforms and upload to the LightBuffer only on profile change.

Coverage note: this demonstrates both **position-only animation** (`player_aura`) and **position + direction animation** (`flashlight`). Both paths update the LightBuffer SSBO via the same `ring-buffered mapped write` path so no barrier changes are needed per-frame.

---

## 6. Fog / volumetrics per profile

Every profile declares a `<fog>` block and a `<volumetrics>` block. The engine's volumetric pass (a 1/2-res ray-march with temporal reprojection and blue-noise dither) reads these uniforms; if `<volumetrics enabled="0"/>` the pass is **skipped entirely**, saving ~0.6 ms on RTX 3080.

| Profile  | Fog type          | Density | Kelvin | Height | Volumetrics | God rays | Steps |
|----------|-------------------|--------:|-------:|-------:|:-----------:|:--------:|------:|
| Sacred   | exp_height        | 0.015   | 6000   | 12     | on          | on       | 48    |
| Wonder   | exp_height        | 0.020   | 5500   | 10     | on          | on       | 64    |
| Dread    | exp_height        | 0.050   | 8000   | 3      | on          | off      | 32    |
| Liminal  | exp               | 0.008   | 4000   | n/a    | off         | off      | —     |
| Hostile  | exp               | 0.012   | 2200   | n/a    | off         | off      | —     |
| Warmth   | exp_height        | 0.025   | 2700   | 6      | on (low)    | off      | 24    |

Only three profiles run volumetric ray-march. Liminal and Hostile disown depth (flat reads), Warmth uses a cheap low-step bed. This is the performance-vs-mood trade stated up front.

---

## 7. Post-FX stack verification

Stack order is engine-fixed (kb §5):

```
[scene offscreen HDR]
  -> tonemap
  -> bloom
  -> grade       (per-profile LUT-ish lift/gamma/gain)
  -> vignette
  -> grain
  -> CRT         (engine-owned — never per-profile)
  -> EVA HUD     (engine-owned — never per-profile)
  -> swapchain
```

Each profile XML specifies parameters for the first five passes *only*. CRT and EVA HUD are owned by the engine and never touched by a profile (charter Pillar 3 — CRT is tonal, not an effect we chain into).

**Barrier-audit interplay:** `/barrier-audit` must show:
- `offscreen_complete` signaled after tonemap-bloom-grade-vignette-grain chain.
- `postprocess_complete` signaled after CRT + EVA compose.
- No implicit layout transitions inside the per-profile chain — each pass reads an image it explicitly transitioned to `SHADER_READ_ONLY_OPTIMAL`.

**Descriptor-dump interplay:** `/descriptor-dump` must show:
- 14 LightBuffer slots populated (see §2 roster).
- Fog volumetric 3D texture bound when active zone is Sacred/Wonder/Dread/Warmth; unbound slot when Liminal/Hostile.
- Per-profile grading LUT (small 3D texture) bound to the grade pass; swapped on zone transition.

---

## 8. Acceptance dependencies & open items

- **Depends on charter v0** — adopted 2026-04-20, so proceeding. If the charter adds a mood pillar later, revisit.
- **Depends on** `schemas/lighting_profile.xsd` existing when `/validate-asset` is run against the six new XMLs. If the XSD is not yet shipped, `/validate-asset` will 404 the dispatcher; this is a pipeline gap, not a content gap.
- **Depends on** engine-side `LightingProfile::load` + `LightingSystem::apply_profile` for zone switching. Until those land, zones are authored and validated but only one profile is active (the scene's `lighting_profile="liminal"` attribute).
- **Does not depend on** PBR — charter forbids cargo-culted PBR. All lighting math remains Lambertian + Blinn-Phong as default; Kelvin temperatures modulate the diffuse color term, not a PBR pipeline.

---

## 9. Pass / fail signals

**Pass (detailed criteria in `lighting_acceptance.md`):**
- `/kelvin-preview` for each authored Kelvin value returns a hex within its palette band.
- `/descriptor-dump` shows 14 LightBuffer entries, all slot indices contiguous, no `VK_NULL_HANDLE`.
- `/light-flicker-tune torch_north …` twice → byte-identical LightBuffer rows.
- `/barrier-audit` shows explicit layout transitions for the volumetric 3D texture and the grade LUT.
- `/mood-apply showcase sacred` swaps fog + grade + vignette params within one frame.
- `/vibe-audit demo/showcase/lighting_profiles/` returns 0 charter drifts.

**Fail (examples):**
- A light at Kelvin 4800 K in the Sacred zone (falls inside the disallowed 4000–5000 band for Sacred) — `/add-light` warning not heeded.
- Flicker producing different values on replay — seed lost or frame-time used instead of tick-index.
- CRT pass runs *before* bloom — stack order corruption.
- `/descriptor-dump` shows the grade LUT at slot N but the shader samples slot N+1.
