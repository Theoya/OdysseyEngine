# Obsidian Sentinel — Cloaked Variant — Build Log

Date: 2026-04-22
Authored by: 3d-asset-modeler agent (subagent run)
Blender: 4.2.20 LTS (portable install at `C:\Users\THadfield\Blender 4.2\`)
Source character: Meshy AI "Obsidian Sentinel biped" zip, 7.4 MB

## Deliverable paths

| File | Purpose |
|---|---|
| `renders/front.png` | 1920×1080 hero render, front view, mid-walk |
| `renders/three_quarter.png` | 1920×1080 hero render, classic 3/4 hero angle |
| `renders/side.png` | 1920×1080 hero render, pure side profile |
| `renders/back.png` | 1920×1080 hero render, back view |
| `../../../../third_party/obsidian_sentinel_cloaked/cloaked_walking.blend` | Baked cloth sim scene — re-render or re-bake from here |
| `../../../../third_party/obsidian_sentinel_cloaked/cloak.blend` | Pre-sim cloak geometry + parented rig |
| `scripts/phase1_import_sanity.py` | Glb import verification |
| `scripts/phase2_cloak_geometry.py` | Cloak mesh authoring |
| `scripts/phase3_cloth_sim.py` | Cloth modifier, wind/turbulence, bake |
| `scripts/phase4_hero_renders.py` | Hero-render lighting + camera sweep |

## Phase timeline

| Phase | Minutes | Outcome |
|---|---|---|
| Phase 1 — Import sanity | 2 | PASS — 24-bone armature, 28353-vert mesh, Walking action grafts cleanly, no NaN over 26 frames |
| Phase 2 — Cloak geometry | 3 | PASS — 884 verts / 825 quads, pin-ring vertex group authored, parented to `Spine` bone, visual placement confirmed behind figure |
| Phase 3 — Cloth sim + bake | ~4 | PASS — bake completed 58 frames (pre-roll + walk + tail), 0 NaN, sane bounding box at mid-walk |
| Phase 4 — Hero renders (pass 1) | 2 | FAIL — rim light blew out as magenta across all surfaces, cape read pink not dark violet |
| Phase 4 — Hero renders (pass 2, lighting fix) | 2 | PASS-with-caveats — body reads as obsidian-metal, cloak reads as dark violet-charcoal, smooth-shading applied to cape |

Total agent wall-clock: ~15 minutes (excluding tool invocation overhead).

## Key technical decisions

- **Walking glb chosen over Casual_Walk glb** — Walking has the shorter, standard-cadence cycle (25-frame loop at 30fps source). Casual_Walk runs 0.8→101.6 = ~3.3 seconds at 30fps, too long for a single hero loop.
- **Hooded collar, not a geometric hood.** The character has horns that protrude up-and-back from the helmet. A typical hood mesh would clip against the horns on every sim frame. Solution: the top ring of cape verts rises ~10 cm above the pin ring, hinting at a stand-up collar that implies "cloak is hooded" without modeling a hood. This avoids horn-collision failure entirely.
- **Cloak arc opens at the FRONT (110° opening).** 250° of arc wraps behind the figure, 110° opens in front. Character faces -Y in world space (verified by the `headfront` bone's forward direction in `debug_facing.py`). Arc centerline at +Y is straight behind.
- **Parented the cloak to the `Spine` bone** (head-local Z=131 cm) rather than to the armature's root. The pin-ring vertices automatically follow the chest, so cloth sim's Pin Group constraint does the work. No per-frame scripted attachment.
- **Thematic wind from behind-below, drifting up-back at ~8 strength.** This keeps the cloak from reading limp at slow walk speeds. A plain gravity-only sim collapses the cape flat against the character's back; the wind teases it OUTWARD so the silhouette reads flowing in every frame. Key insight worth preserving in the skill guide.

## Known defects in the shipped render set

1. **Cloak reads mid-grey-violet, not dark charcoal-violet.** Hem-spill + rim together lift the cape's effective value ~30% above what was authored in the material. Quick-fix path: drop hem_spill energy 35 → 15, and/or darken cloak albedo 0.035 → 0.022.
2. **Side view composition is weak.** Cape eclipses the torso; character reads as "floating horned helmet + arm coming out of a cloak." Quick-fix: push side camera further back (4.5m instead of 3.8m), or move the camera up a bit for a slight looking-down angle that reveals the front opening.
3. **Back view helmet silhouette is muted.** Rim light is the only thing behind the figure, but it's targeting the cape surface, not catching the helmet horns. Quick-fix: add a small cool-white point-light directly above/behind the helmet to separate horns from background.
4. **Cloak top-ring pins show subtle polygonal facets at shoulder** in three_quarter.png (mid-torso). Smooth shading applied but the underlying quad topology has one ring of ~30° dihedral angle between cape-over-shoulder and cape-hanging-down. Auto-smooth at 60° doesn't hide it there. Quick-fix: increase auto-smooth threshold to 90°, or add a 2-segment transition ring in the mesh between pin-ring and drape.

## Blockers / open questions

None. Sim converged, bake completed, renders shipped. No engine-side work attempted (tool-side-only per spec).

## Next iteration hooks

If main-thread Claude's visual QA returns defects, the common-case fixes live in:
- `phase4_hero_renders.py` — lighting rebalance (lines near `add_area_light` calls) and material color tweaks (near `mat_body`/`mat_cloak` definitions). Re-render time: ~2 min.
- `phase3_cloth_sim.py` — if the drape itself looks wrong (too stiff/too loose), adjust `bending_stiffness` (currently 2.5), `mass` (currently 0.3), or wind `strength` (currently 8.0). Full bake re-run: ~1 min.
- `phase2_cloak_geometry.py` — if cape shape needs topology changes (e.g. fuller sweep, shorter hem). Requires re-running Phase 3 after. Combined re-run: ~2 min.
