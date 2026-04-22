# 3D Modeling Skill Library — Index

Persistent library of ordered-recipe procedures for discrete modeling skills. Each guide is a multi-source consensus (5+ tutorials) with linked video sources, ordered steps, Blender 2.93 `bpy` equivalents, and gotchas.

**Protocol:** Read this index before every task. For each needed skill: if the guide exists, follow it; if not, research 5+ tutorials and author the guide BEFORE attempting the task. See `feedback_art_as_skills.md`.

**Priority-1 demo target (from 2026-04-21 user direction):** a half-decent-looking humanoid that walks, built from a pre-existing base model following real tutorials. The skills needed for this demo are starred (★) below.

## Guide status legend
- `complete` — 5+ sources linked, consensus procedure written, `bpy` snippets validated in Blender 2.93
- `in-progress` — being populated by an agent session now
- `template` — file exists, structure is right, but sources and steps still need filling in
- `needed` — on the backlog, no file yet

---

## Character anatomy

- [sculpt_arm](sculpt_arm.md) — model/sculpt a game-ready arm with proper elbow loops — status: needed
- [sculpt_leg](sculpt_leg.md) — model/sculpt a leg with knee deformation loops — status: needed
- [sculpt_torso](sculpt_torso.md) — sculpt a muscular (male) or shapely (female) torso with deformation-ready topology — status: needed
- [sculpt_attractive_face](sculpt_attractive_face.md) — sculpt a heroic/attractive face following proportional canons — status: needed
- [sculpt_hands](sculpt_hands.md) — sculpt a game-ready hand with finger articulation loops — status: needed

## Armor / gear

- [sculpt_armor_plate](sculpt_armor_plate.md) ★ — sculpt a hard-surface armor plate onto a base humanoid via mask-extract / shrinkwrap / solidify / bevel / weighted normal — status: **complete** (2026-04-21)
- [model_visor](model_visor.md) — model a faceted angular visor for a helmet, including glowing emissive slot — status: template
- [model_helmet](model_helmet.md) — sculpt a full helmet shell over a base head — status: needed
- [model_pauldron](model_pauldron.md) — model a dragon-scale or plated shoulder pauldron stack — status: needed
- [model_gauntlet](model_gauntlet.md) — model an armored glove/gauntlet with optional claws — status: needed
- [model_boot](model_boot.md) — model a heavy wedged/hooved boot — status: needed
- [model_cloak_geometry](model_cloak_geometry.md) — parameterized polar-grid cape mesh with pin-ring vertex group — status: **complete** (2026-04-22; single-execution auth'd, Meshy Obsidian Sentinel cloaked pass)

## Cloth / physics authoring

- [setup_cloth_simulation_blender](setup_cloth_simulation_blender.md) — Blender 4.2 Cloth modifier tuning + bake protocol for a character cape — status: **complete** (2026-04-22)
- [add_thematic_wind_to_cloak](add_thematic_wind_to_cloak.md) — Wind + Turbulence force-field trick to keep a cape from reading limp — status: **complete** (2026-04-22)
- [attach_cloak_to_humanoid](attach_cloak_to_humanoid.md) — bone-parent a cloak to a chest bone so pin-ring rides the torso — status: **complete** (2026-04-22)

## Rigging / deformation

- [rig_shoulder](rig_shoulder.md) — set up a shoulder joint with proper weight falloff — status: needed
- [weight_paint_elbow](weight_paint_elbow.md) — weight-paint an elbow for clean deformation — status: needed
- [weight_paint_knee](weight_paint_knee.md) — weight-paint a knee for clean deformation — status: needed
- [setup_ik_chain](setup_ik_chain.md) ★ — set up IK on an arm or leg chain — status: needed
- [skin_base_to_skeleton](skin_base_to_skeleton.md) ★ — skin a base humanoid mesh to a target skeleton (bone heat / envelope / transfer weights) — status: **complete** (2026-04-21; MPFB 53-bone → engine 19-bone mapping table derived from `rig.game_engine.json`)

## Animation

- [animate_walk_cycle](animate_walk_cycle.md) ★ — author a 4-key walk cycle (contact / down / passing / up) on a rigged humanoid — status: **complete** (2026-04-21; 9 sources, XYZW-vs-WXYZ quaternion gotcha + retarget workflow)
- [animate_idle](animate_idle.md) — author a subtle breathing/idle loop — status: needed
- [animate_run_cycle](animate_run_cycle.md) — author a run cycle with higher cadence + ballistic contact — status: needed
- [animate_reach_pose](animate_reach_pose.md) — author a reach animation for picking up or pointing — status: needed
- [pose_character_static](pose_character_static.md) — pose a rigged character for a static beauty render — status: needed

## Posture / stance

- [pose_heroic_stance](pose_heroic_stance.md) — heroic contrapposto or power stance — status: needed
- [pose_combat_ready](pose_combat_ready.md) — weight forward, weapon at guard, crouched slightly — status: needed
- [pose_hunched_menacing](pose_hunched_menacing.md) — forward hunch, shoulders raised (used on Berserk-Halo) — status: needed
- [pose_relaxed_idle](pose_relaxed_idle.md) — neutral weight, slight S-curve, arms relaxed — status: needed

## Surface / texture

- [bake_normal_map](bake_normal_map.md) — bake a high-poly sculpt onto a low-poly retopo normal map — status: needed
- [uv_unwrap_character](uv_unwrap_character.md) — UV-unwrap a humanoid with minimal stretch / good pack — status: needed
- [author_matxml_from_blender](author_matxml_from_blender.md) — translate a Blender material to an OdysseyEngine `.mat.xml` — status: needed

## Pipeline

- [install_mpfb2](install_mpfb2.md) ★ — install MPFB2 extension into Blender 4.2+ — status: **complete** (verified end-to-end 2026-04-21; MPFB 2.0.15 build 20260421; dual-Blender install unblocks this)
- [generate_base_male](generate_base_male.md) ★ — generate a muscular attractive CC0 male base via MPFB2 — status: **complete** (2026-04-21; includes gender-slider inversion gotcha)
- [generate_base_female](generate_base_female.md) ★ — generate a shapely attractive CC0 female base via MPFB2 — status: **complete** (2026-04-21)
- [import_rigged_fbx](import_rigged_fbx.md) — import a rigged FBX into a Blender scene — status: needed
- [scale_to_engine_skeleton](scale_to_engine_skeleton.md) ★ — normalize a base mesh to match `humanoid.skeleton.xml` (1.8m, root Y=0.95) — status: **complete** (2026-04-21)
- [retarget_rig](retarget_rig.md) ★ — retarget a base mesh's native rig to the engine's 19-bone skeleton — status: needed (paused agent plans to author)
- [write_mesh_xml_descriptor](write_mesh_xml_descriptor.md) — author an engine-valid `.mesh.xml` pointing at an external OBJ — status: **complete** (2026-04-21)
- [render_hero_shot_etherealism](render_hero_shot_etherealism.md) ★ — Eevee render settings for Fantasy Etherealism / ship-vibe hero shots — status: **complete** (2026-04-21; 12 renders delivered)

## Meshy AI pipeline (weapons/tools/handheld — default generator per `project_meshy_pipeline.md`)

- [meshy_api_integration](meshy_api_integration.md) — auth, endpoints, polling, download, error handling for Meshy Pro REST API — status: needed
- [generate_weapon_meshy](generate_weapon_meshy.md) — prompt-authoring + API call + decimate-retopo pass for weapons (rifles, pistols, blades, staves) — status: needed
- [generate_ornate_component_meshy](generate_ornate_component_meshy.md) — pommels, emitters, rune engravings, decorative prongs via Meshy — status: needed
- [retopo_meshy_output](retopo_meshy_output.md) — clean up Meshy's dense 200-500k-tri output into game-ready quads — status: needed
- [kitbash_mechanical_firearm](kitbash_mechanical_firearm.md) — combine Meshy-generated ornate parts with hand-modeled mechanical parts (receiver, barrel, trigger) into a complete weapon — status: needed
