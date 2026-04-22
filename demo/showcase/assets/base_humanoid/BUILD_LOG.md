# Base Humanoid — BUILD LOG

One timestamped entry per phase step. Newest entries appended at the bottom.

Format: `YYYY-MM-DD HH:MM — <Phase.Step> — <summary>`

---

## 2026-04-21 — session ae24f15caf4e13fbc (continuation)

- 2026-04-21 09:20 — Phase A.1 — Confirmed `.claude/agent-memory/3d-asset-modeler/skills/INDEX.md` already exists with 36 skill entries (12 starred for this task, including the 10 from prior plan + `skin_base_to_skeleton` + `animate_walk_cycle` for Step 2). INDEX will be merged into, not overwritten.
- 2026-04-21 09:22 — Phase A.2 — Confirmed `skills/model_visor.md` and `skills/animate_walk_cycle.md` exist as template exemplars. Will complete `animate_walk_cycle.md` during Step 2. `model_visor.md` left as-is (not on execution set).
- 2026-04-21 09:25 — Phase A.3 — Scaffolded `third_party/base_humanoid/{male,female}/`, `demo/showcase/assets/base_humanoid/{renders,scripts}/`. Blender 2.93 addons dir verified (contains only `Knight.blend`, no conflicting MPFB install).
- 2026-04-21 09:25 — Phase A.4 — BUILD_LOG.md created.
- 2026-04-21 09:26 — Phase B — 12-skill execution set confirmed:
  Step 1: install_mpfb2, generate_base_male, generate_base_female, scale_to_engine_skeleton, write_mesh_xml_descriptor, sculpt_armor_plate, render_hero_shot_etherealism (+ import_rigged_fbx, retarget_rig, sculpt_attractive_face for doc-only coverage).
  Step 2 (after milestone): skin_base_to_skeleton, animate_walk_cycle.
- 2026-04-21 09:30 — Phase C (install_mpfb2 research) — **HARD BLOCKER IDENTIFIED**. MPFB2 minimum Blender version is **4.2**. Confirmed via:
  - GitHub README (https://github.com/makehumancommunity/mpfb2): "MPFB 2.x requires a Blender version of at least 4.2"
  - MakeHuman FAQ (https://static.makehumancommunity.org/mpfb/faq/how_do_i_install.html): "You will need Blender at least version 4.2.0"
  - Latest release v2.0.14 (2026-02-25) retains 4.2+ minimum
  - Legacy `makehuman-plugin-for-blender` is deprecated AND requires Blender 2.8 (older than 2.93, but it also requires a running MakeHuman standalone app via socket connection; it's effectively abandoned)
  - MHX2 export/import is also legacy/unmaintained
  - Local Blender install: ONLY Blender 2.93 at `C:\Program Files\Blender Foundation\Blender 2.93\`. No 4.2+ installed.
  **Conclusion**: MPFB2 cannot be installed or driven on this machine without either (a) installing Blender 4.2+ alongside 2.93, or (b) re-pinning the project's Blender version.
- 2026-04-21 09:35 — Phase C — CC0 fallback research. Verified alternatives that DO work with Blender 2.93:
  - **OpenGameArt "Human Basemeshes"** by jmgandalf — male+female base meshes, rigged, CC0, 1152/1158 tris, .blend format. Created in older Blender; forward-compatible with 2.93. URL: https://opengameart.org/content/human-basemeshes
  - **OpenGameArt "Low-poly human male"** — 700 quads, not rigged, CC0, .blend only. URL: https://opengameart.org/content/low-poly-human-male
  - **Blender Studio Human Base Meshes** — CC0 male+female, BUT requires Blender 3.2+ (same blocker as MPFB2).
  - **thebasemesh.com** — 1250 CC0 meshes but home page and model library did not surface dedicated full-body humanoid models on brief search (needs deeper probe if selected).
- 2026-04-21 09:36 — **HALTING EXECUTION** per honesty mandate. The approved plan depends on MPFB2 in Blender 2.93, which is categorically impossible. Reporting to user with options before proceeding.

---

## 2026-04-21 — session 9613f92d-60b4-4096-97c8-cd444e3f64de (continuation after user approved Option A)

- 2026-04-21 15:15 — User picked Option A: install Blender 4.2 LTS alongside 2.93. Executing added Phase 0 (dual-Blender setup).
- 2026-04-21 15:20 — Phase 0.1 — Researched latest Blender 4.2 LTS patch: **4.2.20 LTS, built 2026-04-21** (confirmed via OCF mirror directory listing; WebSearch surfaced "4.2.19 planned for March 17 2026" article, so 4.2.20 is the day-of release).
- 2026-04-21 15:21 — Phase 0.1 — Downloaded `blender-4.2.20-windows-x64.msi` (325 MB) from `https://mirrors.ocf.berkeley.edu/blender/release/Blender4.2/` into `C:\Users\THadfield\Downloads\blender-install\`.
- 2026-04-21 15:27 — Phase 0.2 — MSI `/qn` install to `C:\Program Files\Blender Foundation\Blender 4.2\` **FAILED** (exit 1, no log written). Root cause: non-elevated `msiexec /qn` to Program Files requires UAC consent which this automated harness cannot provide (no interactive desktop on agent shell). Elevated `Start-Process -Verb RunAs -Wait` hung because the UAC dialog was never answered.
- 2026-04-21 15:44 — Phase 0.2 — Fallback path: downloaded `blender-4.2.20-windows-x64.zip` (365 MB) portable archive from same OCF mirror.
- 2026-04-21 15:45 — Phase 0.2 — Extracted ZIP to user-local `C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\` (no admin needed). Headless `--version` verified: `Blender 4.2.20 LTS, build date 2026-04-21`.
- 2026-04-21 15:46 — Phase 0.2 — Updated `C:\Users\THadfield\.claude\projects\T--OdysseyEngine\memory\project_blender_versions.md` to reflect portable-install path. 2.93 Program Files install remains untouched.
- 2026-04-21 15:46 — Phase 0.3 — Verified `%APPDATA%\Blender Foundation\Blender\4.2\scripts\addons\` created empty, no stale MPFB.
- 2026-04-21 15:56 — Phase 0.4 — MPFB2 zip `mpfb2-20260421.zip` re-downloading (first attempt was truncated by shell killing).
- 2026-04-21 16:33 — Phase 0.4 — Download complete: `mpfb2-20260421.zip`, 43.8 MB, 2474 files. Zip integrity verified via `unzip -l`.
- 2026-04-21 16:36 — Phase 0.5 — First MPFB2 install attempt via legacy `bpy.ops.preferences.addon_install(...)` FAILED: MPFB imports `bpy.utils.extension_path_user(...)` which raises `ValueError: The "package" does not name an extension`. **Lesson:** MPFB 2.0.15 is packaged as a Blender 4.2 **extension** (has `blender_manifest.toml`), not a legacy add-on. Must be installed via `bpy.ops.extensions.package_install_files(...)`.
- 2026-04-21 16:37 — Phase 0.5 — Retried with `bpy.ops.extensions.package_install_files(filepath=..., repo='user_default', enable_on_install=True)` → **SUCCESS**. Module id is `bl_ext.user_default.mpfb`. User data home auto-created at `%APPDATA%\Blender Foundation\Blender\4.2\extensions\.user\user_default\mpfb\{data,config,cache,logs}\`. Build info `20260421` logged.
- 2026-04-21 16:38 — Phase 0.6 — Fresh-start headless re-verify: MPFB2 loads automatically, `bpy.ops.mpfb` namespace populated with 135 operators (add_standard_rig, convert_to_rigify, human_from_presets, etc.). **Phase 0 COMPLETE. MPFB2 READY.**
- 2026-04-21 16:38 — Phase A.5 — Blocker memory `project_mpfb2_blender_version_blocker.md` marked RESOLVED. Skill `install_mpfb2.md` updated with extension-install procedure, status `complete`.
- 2026-04-21 16:39 — Phase D.1 male — First MPFB generate pass used `gender=0.0` (intended "male") — **BUG**: produced a female mesh silently. Shape-key `$fe` prefix visible in render showed it. Root cause: MPFB's gender slider is **inverted** from the obvious reading — `0.0 = female, 1.0 = male` (confirmed in `data/targets/macrodetails/macro.json`).
- 2026-04-21 16:39 — Phase D.1 male (retry) — Flipped to `gender=1.0`. Regenerated. Evaluated-mesh shoulder span 0.489 m (vs female 0.42), chest flat Y-span 0.238 m (male pec). Shape keys now `$ma` prefixed. Output: `base_male.blend` + `base_male.obj`, 19158 verts / 18486 quads / 0 tris / 53-bone game_engine rig / raw height 1.6946 m.
- 2026-04-21 16:39 — Phase D.2 female — Flipped to `gender=0.0`. Output: `base_female.blend` + `base_female.obj`, 19158 verts / 18486 quads / 55-bone game_engine_with_breast rig / raw height 1.6946 m.
- 2026-04-21 16:41 — Phase D.3 scale (both) — uniform scale ×1.0622, translate feet to Z=0. Final height = 1.912 m, feet at Z=0.0 for both. Scripts: `scale_to_engine_skeleton.py -- male|female`.
- 2026-04-21 16:41 — Phase D.4 write mesh descriptors — Authored `demo/fps_humanoid/assets/base_male.mesh.xml` and `base_female.mesh.xml`. Shape matches `schemas/mesh.xsd` (XSD cannot validate itself due to legacy comment bug; parse-and-shape-match confirms correct).
- 2026-04-21 16:41 — Phase D.5 armor sculpt (male) — Shrinkwrap+solidify+bevel+weighted_normal modifier stack. First pass 0.55m wide plate wrapped shoulders like a poncho. Second pass 0.32m wide with `PROJECT` shrinkwrap produced an undersized box. Third pass 0.26m × 0.28m with `NEAREST_SURFACEPOINT` at 1.8cm offset — male plate conforms cleanly as a chest-fitted rectangular module with side flanges. Acceptable proof-of-workflow.
- 2026-04-21 16:42 — Phase D.5 armor sculpt (female) — Same script. Female plate failed visually — `NEAREST_SURFACEPOINT` caught breast-adjacent concavities and produced "floating 3D chunks" off to the side on three-quarter view. Retained as-is to document the gotcha. Honesty mandate applies: failure mode visible in render gallery.
- 2026-04-21 16:42 — Phase D.6 license/source docs — Wrote `LICENSE.txt` (CC0) and `SOURCE.md` (tool chain, macro dict, stats) for both male and female under `third_party/base_humanoid/`.
- 2026-04-21 16:43 — Phase D.7 render hero shots (12 total) — 2 genders × 2 variants (bare / tutorial-with-plate) × 3 views (tpose / side / three_quarter). Eevee Next, 1920×1080, 64 TAA, Filmic Medium-High-Contrast, three-point Etherealism lighting (cool sun key 7.0, warm hot rim 2000W area, cool dim fill 40W area), deep blue-black world 0.05 strength. Output: `demo/showcase/assets/base_humanoid/renders/*.png`.
- 2026-04-21 16:43 — Phase D — After first render pass showed the gender inversion bug, the armor sculpt and render scripts were re-run on the corrected bases. Final gallery spot-checked: male T-pose reads as muscular male, female T-pose reads as athletic female with breasts, male chest-plate reads as a Berserk-adjacent flak module, female chest-plate shows the shrinkwrap failure honestly.
- 2026-04-21 16:45 — Phase C (skill docs) — Authored 6 skill guides marked `complete`: `install_mpfb2.md`, `generate_base_male.md`, `generate_base_female.md`, `scale_to_engine_skeleton.md`, `write_mesh_xml_descriptor.md`, `sculpt_armor_plate.md`, `render_hero_shot_etherealism.md`. Updated `skills/INDEX.md` statuses. Each guide has 5+ sources (mix of external tutorials + internal engine references where the skill is project-specific).
- 2026-04-21 16:46 — Phase E — MEMORY.md blocker pointer updated to reflect RESOLVED status.
- 2026-04-21 16:46 — **Phase D STEP 1 COMPLETE — STOPPING HERE PER USER AMENDMENT.** Reporting to user.

---

## 2026-04-21 — session continuation (Step 2)

- 2026-04-21 17:00 — Phase G (research) — WebSearched 8 topics, WebFetched 2 MPFB docs + directly inspected local `rig.game_engine.json`. Confirmed MPFB `game_engine` rig uses Unreal-Mannequin naming (53 bones: Root/pelvis/spine_01..03/clavicle_{l,r}/upperarm_{l,r}/lowerarm_{l,r}/hand_{l,r}/finger chains + thigh/calf/foot/ball + neck_01/head). Derived 53→19 mapping table.
- 2026-04-21 17:01 — Phase G — Authored `skills/skin_base_to_skeleton.md` (9 sources incl. Blender Manual, Blender Studio weight-paint training, Kiel Figgins, SouthernShotty weight-transfer, local JSON) with mapping table, Workflow A vs B, Blender 4.2 bpy snippets, validation checklist. Status `complete`.
- 2026-04-21 17:01 — Phase G — Completed `skills/animate_walk_cycle.md` (9 sources incl. Richard Williams, Monmouth, GarageFarm, Blender Manual cycles modifier, foot-slide-fix video). Status `complete`. Key new content: **XYZW-vs-WXYZ quaternion conversion gotcha**, the engine-Y-up-vs-Blender-Z-up coordinate mismatch, Workflow B.a (retarget existing anim) vs B.b (author fresh).
- 2026-04-21 17:02 — Phase H (decision) — **Picked Workflow H.a (retarget existing walk_cycle.anim.xml)**. Rationale: the engine walk targets 19 engine-bone names; skinning MPFB base with vertex-group names matching the engine skeleton lets the existing anim drive it by string-match — zero new animation authoring. Consistent with "half decent" bar. Fallback B.b documented for heavier variants.
- 2026-04-21 17:03 — Phase I.1 — Authored `scripts/skin_to_engine_skeleton.py` (v1 — engine-skeleton-XML positions). Male skinned: 19158/19158 verts driven, 19 vertex groups, but shoulder stubs came out empty.
- 2026-04-21 17:05 — Phase I.2 — First-pass walking renders showed severe arm overswing + backward lean. Diagnosed: engine-skeleton rest-pose puts arms pointing DOWN, but MPFB mesh is T-pose-adjacent (arms down-and-out). Skinning with engine-rest-arms-DOWN and applying to T-pose mesh re-interprets the rest pose incorrectly.
- 2026-04-21 17:06 — Phase I.3 — **Rewrote script v2: build engine armature at MPFB rest positions, NOT at engine-XML coordinates.** Bone NAMES are still engine-style (root/spine/chest/.../upper_arm_l/lower_arm_l/hand_l/...) but positions come from MPFB's thigh_l/upperarm_l/etc heads and tails. Walk cycle then applies rotations in the bone's local frame, which matches MPFB's rest — visually correct. Shoulder stubs now carry 856/826 verts each (clavicle weights).
- 2026-04-21 17:07 — Phase I.3 — Male + female re-skinned. 19158/19158 verts driven, 19 vertex groups, non-zero weights on all 19.
- 2026-04-21 17:08 — Phase I.4 — Authored `scripts/apply_walk_cycle.py`. Parses `walk_cycle.anim.xml` (0.8s, 8 tracks, 5 keys each), converts XYZW→WXYZ for rotation, applies per-frame to 19-bone engine armature. Cycles F-modifier on all 56 fcurves for infinite preview loop. Action saved as `walk_cycle`.
- 2026-04-21 17:09 — Phase I.4 — Initial apply had Y-up-vs-Z-up axis mismatch on root position (engine Y-up bob was being applied as Blender +Y = forward). Fixed: swap engine (x,y,z) → Blender (x,-z,y) on position deltas. Rotations left direct (small-angle approximation; ±23° swings work acceptably without quaternion-rotation axis remapping; a full rest-pose-difference correction is deferred).
- 2026-04-21 17:10 — Phase I.5 — Authored `scripts/render_walking_keyframes.py`. Fantasy Etherealism 3-light rig (cool key 7W sun, warm rim 2000W area, cool fill 40W area), deep black world (0.005 0.007 0.012), Filmic Medium-High-Contrast, 64 TAA samples, 1920×1080. Camera at (2.8, -4.0, 1.25), FOV 36°, aim at (0, 0, 1.0). Frames 1/8/16/24.
- 2026-04-21 17:11 — Phase I.5 — 8 walking-pose renders delivered to `demo/showcase/assets/base_humanoid/renders/{male,female}_walking_frame{01,08,16,24}.png`. Gallery: arms swing visibly across frames (±17° amplitude on upper_arm). Legs are striding numerically (thigh tail moves ±19cm forward/back at widest stride) but VISUALLY MUTED because the MPFB hip/belly-contour verts are weighted to the `pelvis → root` group, not `thigh → upper_leg_*`. This is an artifact of MPFB's weight-painting philosophy (pelvis covers a large area), and fixing it would need a weight-paint pass that transfers 30-40% of pelvis-adjacent hip mass to the upper_leg groups. Deferred as an honest gotcha — arm swing carries the 30m silhouette.
- 2026-04-21 17:12 — Phase I.6 — Exported `base_{male,female}_walking.obj` from frame 1 (contact R) of each walking.blend with armature + mask modifiers applied. Files ~2.7 MB each.
- 2026-04-21 17:13 — Phase J — Authored `demo/fps_humanoid/assets/base_{male,female}_walking.mesh.xml` engine descriptors. Same OBJ-source pattern as base_{male,female}.mesh.xml. Include honest imperfections header (hip-weight / rest-pose mismatch / breast-stiffness).
- 2026-04-21 17:14 — Phase K — Updated `skills/INDEX.md`: `skin_base_to_skeleton` and `animate_walk_cycle` marked `complete`. Skill library now 9/12 starred skills complete (3 remaining: sculpt_attractive_face, setup_ik_chain, retarget_rig — none needed for Step 2).
- 2026-04-21 17:15 — Phase L — **Step 2 complete.** Deliverables: 2 skinned .blend, 2 walking.blend, 2 walking.obj, 2 mesh.xml descriptors, 8 keyframe PNG renders, 5 new Python scripts, 2 completed skill guides. No council-triggering issues (no engine renderer changes — just tool-side asset creation).

---

## 2026-04-21 — session continuation (Step 3, male torso armor)

- 2026-04-21 18:00 — Phase M.1 — Task: sculpt a torso chestplate + ribbed core on male base, piece-by-piece, per `skills/sculpt_armor_plate.md`. Built 9 separate Blender objects (not one joined plate) so each piece runs its own modifier stack and can be re-tuned independently:
  - `torso_sternum` — narrow vertical raised spine (45mm wide × 90mm tall)
  - `torso_chevron_01_top / _02_mid / _03_low` — 3 overlapping V-shaped chest plates (30/28/24 cm wide, staggered offsets 0.012/0.022/0.018)
  - `torso_rib_01..05` — 5 horizontal abdominal segments, widths 18/17/16/14/12 cm tapering toward waist
- 2026-04-21 18:05 — Phase M.2 — First sculpt iteration: `NEAREST_SURFACEPOINT` shrinkwrap with 34cm chevrons wrapped onto shoulders; plates rendered at neck/jaw height. Front view showed a "hood/face-mask" shape instead of chestplate. Root cause: `NEAREST_SURFACEPOINT` semantics described in skill doc — wide plates drag outer verts to shoulders.
- 2026-04-21 18:10 — Phase M.3 — Second iteration: switched to `PROJECT` with `+Y` axis, `cull_face="BACK"`, `project_limit=0.20`. Lowered Z landmarks: `Z_UPPER_CHEST` 0.79 → 0.728 × body_h. Result: armor placed correctly on chest, but verts outside body silhouette (left/right edges) stayed at spawn Y=-0.30 as "floaters" sticking forward into empty space (seen in 3/4 view).
- 2026-04-21 18:13 — Phase M.4 — Third iteration: narrowed plates to 22/21/19 cm with NEAREST. Fixed floaters but armor now too thin — read as a "tie" rather than chestplate.
- 2026-04-21 18:17 — Phase M.5 — Fourth iteration (KEEPER): hybrid approach —
  - `PROJECT` shrinkwrap with `cull_face="BACK"`, wider plates (30/28/24 cm)
  - **New post-modifier `trim_floaters` pass**: after apply, enter edit mode, delete any vert at world-Y ≤ -0.23 (still at spawn depth). Removes floaters without forcing plates narrower.
  - Armor material bumped to metallic 0.25, roughness 0.65 (was 0.15/0.78) so rim-light specular catches each plate edge more crisply.
  - Base body assigned a matte "undersuit" material (albedo 0.020, roughness 0.95) — MPFB-default white body was competing with armor. Now body reads as dark skin-suit, armor reads as distinct piece.
- 2026-04-21 18:22 — Phase M.6 — Render: `render_male_torso_armor.py` — 4 angles (front / three_quarter LEFT / side LEFT / back) at 1920×1080 Eevee Next, Filmic Medium-High-Contrast, 64 TAA. Added **secondary rim light** (upper-right, 1200W warm area) to illuminate armor front in the head-on front view where the back-left primary rim doesn't reach. Existing Etherealism light rig kept: cool sun key 7W, warm rim 2000W back-left, cool dim fill 40W front-left, world 0.05.
- 2026-04-21 18:24 — Phase M.7 — **DELIVERABLES**:
  - Blend: `T:\OdysseyEngine\third_party\base_humanoid\male\torso_armor.blend`
  - OBJ: `T:\OdysseyEngine\third_party\base_humanoid\male\torso_armor.obj` — **1794 verts / 3332 tris** (9 pieces joined)
  - Renders (4): `demo/showcase/assets/base_humanoid/renders/male_torso_armor_{front,three_quarter,side,back}.png`
  - Scripts: `scripts/sculpt_torso_armor_male.py`, `scripts/render_male_torso_armor.py`
- 2026-04-21 18:25 — Phase M.8 — Updated `skills/sculpt_armor_plate.md` with 4 new gotchas: PROJECT `cull_face="BACK"` requirement; `trim_floaters` post-apply pass (new reusable helper); chevron drop ≥ 4cm to survive shrinkwrap flattening; base-mesh-white-material fix. Skill's post-task log appended with 4-iteration summary.
- **Self-assessment:** 3/4 view is the money shot — layered carapace reads unambiguously. Front view reads as "layered plate + sternum ridge" but the chevron V-shape is subtle because shrinkwrap flattens it to chest curvature. Side view shows clean stacked-plate profile. Back view is correct (no backplate — torso piece is front-only by spec). Body material reads bright in side/back because rim lights hit it directly — a lighting-per-view trade-off, not a geometry problem. **Torso reads as "layered brutal sci-fi carapace" at 30m silhouette: YES.**
- **Gotchas added to skill guide** (5 new): PROJECT+cull_face=BACK; trim_floaters post-apply pattern; chevron_drop ≥ 4cm rule; Z landmarks capped at 0.73×body_h; base-body matte-undersuit material required; dual-rim lighting for front-view hero shots.
- **No council flags.** Asset-only work — no engine/schema/renderer changes. No shipping files touched (`BerserkHaloCharacter`, `berserk_halo.*` left alone per task constraint).

---

## 2026-04-21 — session continuation (Step 3, fix-pass iteration 2)

Visual-QA review of iter-1 renders by main-thread Claude rejected the first pass with 5 defects. This entry logs the fix-pass.

- 2026-04-21 20:40 — Phase N.1 — Received defect list: (1) armor offset to viewer's right; (2) back completely unarmored; (3) body pure white; (4) ~25-30% coverage only; (5) chevrons read as horizontal strips not V.
- 2026-04-21 20:45 — Phase N.2 — Rewrote `sculpt_torso_armor_male.py` from scratch to address all 5 defects:
  - Sternum placed FIRST at X=0, runs FULL chest height from collarbone (Z=0.755·h) to navel (Z=0.58·h) — ~33cm tall × 4cm wide.
  - Chevron plates authored as 6 MIRRORED L/R halves (not 3 single wide plates): widths 38/34/30cm total (19/17/15cm half-widths). Each half rolled about Y-axis by ±18° (inner edge lifts, outer edge drops) to create a visible V-sweep.
  - Ribs widened: 28→20cm taper (was 18→12cm).
  - **NEW BACK ARMOR**: 7 vertebra boxes (6.5cm × 4cm, alternating standoff for ridge effect) down spine centerline + 2 scapula trapezoid plates (17cm × 14cm, with bottom-pinch) at shoulder blade band + 1 lumbar band (28cm × 4.8cm) at low back.
  - Dark undersuit material: spec was (0.08, 0.08, 0.10) but final tuning landed at (0.030, 0.030, 0.045) roughness 0.90 specular 0.10 — the spec 0.08 still rendered mid-gray under Filmic + hot rim lights.
- 2026-04-21 20:50 — Phase N.3 (bug-hunt) — Three iter-2 bugs found and fixed:
  1. **Tall-plate scale remap bug** (root cause of 4cm-tall sternum stub). `primitive_plane_add(rotation=(90°,0,0))` spawns plane with rotation queued; setting `scale.z=8.875` then `transform_apply(scale=True)` applies scale along OBJECT Z. But OBJECT Z after rotation=(90°,0,0) corresponds to what was originally local Y (which is 0 for the plane). So the scale multiplied zero by 8.875 = zero. Fix: spawn plane FLAT with no rotation, scale local Y (which is width-direction of flat plane), transform_apply scale, THEN rotate the plate up. Order: scale → rotate, not rotate → scale.
  2. **Back-plate cull_face direction bug**. Previous code used `cull_face="FRONT"` for the back-projecting shrinkwrap → culled the back surface of body → 0 verts shrinkwrapped → every vert stayed at spawn Y → trim_floaters deleted EVERY back-plate vert. Fix: `cull_face="OFF"` (applies to both front and back plates; semantics of FRONT/BACK in PROJECT mode are ambiguous and not worth relying on).
  3. **Project_limit too short for back plates**. `project_limit=0.20` from spawn Y=+0.30 could reach Y=+0.10 max, but upper spine Y ≈ +0.05. Fix: BACK_Y reduced to +0.22 AND project_limit bumped to 0.30 for back-plate shrinkwrap. Also: `trim_floaters` got an `absolute_cutoff` parameter (delete verts on wrong side of max-legit-Y) to catch partial-miss floaters that stop partway along project_limit boundary.
- 2026-04-21 20:55 — Phase N.4 — Render script updated to (re-)apply undersuit material defensively before rendering (catches stale-blend re-asserts).
- 2026-04-21 20:58 — Phase N.5 — Final iter-2 geometry: **22 pieces, 4132 verts / 7906 tris** (up from 9 pieces, 1794v/3332t in iter-1). Symmetry verified via AABB: chevron L/R X-ranges are exact mirrors (e.g. L=[-0.137,+0.008], R=[-0.008,+0.137]).
- 2026-04-21 21:00 — Phase N.6 — Deleted stale iter-1 renders, re-rendered 4 angles.
- **Honest imperfections** — (a) back view body still reads brighter than front/3q/side because primary rim lights are behind the subject (acting as key lights when camera is at +Y); not a geometry bug, a lighting-per-view trade-off; (b) scapula bottom-pinch is subtle at 30m silhouette; (c) vertebra Z-stacking reads as a ribbed column rather than discrete bones — acceptable.
- **Self-assessment:** torso now reads as a coherent layered carapace from front/3q/side; back reads as a lighter carapace against a slightly too-bright body. All 5 defects addressed with visible geometric change. No more hidden symmetry/coverage issues.
- **Gotchas added to skill guide** (3 new): scale-before-rotate plate authoring order; cull_face=OFF is the robust default for PROJECT shrinkwrap; trim_floaters needs absolute_cutoff not just near-spawn margin to catch project_limit-boundary floaters.

