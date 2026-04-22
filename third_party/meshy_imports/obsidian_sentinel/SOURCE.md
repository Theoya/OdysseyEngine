# Obsidian Sentinel — Meshy AI Source Provenance

## Acquisition
- **Source:** Meshy AI (https://www.meshy.ai) — paid user account generation
- **User / License holder:** taylorhadfield1@gmail.com
- **Download date:** 2026-04-22 (user-downloaded)
- **Original zip:** `Meshy_AI_Obsidian_Sentinel_biped.zip` (7.4 MB, 7 files)

## Inferred generation prompt
From the filename `Meshy_AI_Obsidian_Sentinel_biped` the prompt likely contained
the phrase **"obsidian sentinel"** with a biped rig flag enabled. Meshy's
"AI Texture + Rig + Animate" pipeline yields:
1. Character_output.glb — the base rigged mesh (T-pose or A-pose)
2. N × Animation_<name>_withSkin.glb — each selected library animation
   baked into its own file, with the skinned mesh included

## Files
| File | Bytes | Purpose |
|---|---|---|
| `Meshy_AI_Obsidian_Sentinel_biped_Character_output.glb` | 1,679,572 | Base rigged character (use as authoring source) |
| `..._Animation_Agree_Gesture_withSkin.glb` | 1,841,132 | Head-nod / approve gesture animation |
| `..._Animation_Axe_Stance_withSkin.glb` | 1,758,028 | Two-handed axe ready stance |
| `..._Animation_Block1_withSkin.glb` | 1,718,720 | Shield/defensive block |
| `..._Animation_Casual_Walk_withSkin.glb` | 1,729,848 | Relaxed walk (slower cadence) |
| `..._Animation_Running_withSkin.glb` | 1,687,700 | Full-speed run cycle |
| `..._Animation_Walking_withSkin.glb` | 1,692,324 | Standard walk cycle |

## Chosen authoring animation for cloak sim
**`Walking`** (the standard walk, not Casual_Walk) — selected during Phase 1
after inspecting both. Rationale and per-file observations recorded in
`demo/showcase/assets/obsidian_sentinel_cloaked/BUILD_LOG.md`.

## Usage policy
See `LICENSE.txt` in this directory. Derivative (cloaked / modified) outputs
for engine consumption are under `demo/showcase/assets/obsidian_sentinel_cloaked/`.
