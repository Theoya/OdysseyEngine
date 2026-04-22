---
skill: skin_base_to_skeleton
difficulty: intermediate
prerequisites: [generate_base_male, generate_base_female, scale_to_engine_skeleton]
target_blender: 4.2 LTS (portable at C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\blender.exe)
status: complete — authored 2026-04-21
---

## Goal

Take a CC0 base humanoid mesh that was generated with the MPFB2 `game_engine` rig (53 bones, UE4-Mannequin naming — `pelvis`, `spine_01..03`, `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`, `thigh_l/r`, `calf_l/r`, `foot_l/r`, `neck_01`, `head`, plus 5 finger chains per hand + a `Root`) — and skin it to the OdysseyEngine 19-bone humanoid skeleton (`demo/fps_humanoid/assets/humanoid.skeleton.xml`), so that the existing engine walk animation (`walk_cycle.anim.xml`) drives the MPFB mesh without re-authoring the animation.

The 19-bone engine skeleton has one armature bone per named bone in the XML; finger chains, clavicle, and "ball-of-foot" bones do NOT exist on the engine rig, so we merge the corresponding MPFB bones into their nearest engine-skeleton parent when assigning weights.

## Sources (5+ required — collected 2026-04-21)

1. [Blender Manual — Armature Deform Parent (Ctrl+P → With Automatic Weights)](https://docs.blender.org/manual/en/latest/animation/armatures/skinning/parenting.html) — 2026 — definition of bone-heat weighting and the `ARMATURE_AUTO` parent type, including requirements (T-pose, closed mesh, bone alignment).
2. [Blender Manual — Data Transfer Modifier](https://docs.blender.org/manual/en/latest/modeling/modifiers/modify/data_transfer.html) — 2026 — canonical reference for copying `VGROUP_WEIGHTS` from a proxy mesh to a new mesh when topologies differ. Covers the Generate Data Layers step and mapping mode choices (`POLYINTERP_NEAREST`, `NEAREST` etc.).
3. [Transfer Weights From One Mesh to Another in Blender (YouTube, SouthernShotty)](https://www.youtube.com/watch?v=yOy6NgwRPaI) — 2022 — practical walkthrough: choose source, enable "All Layers" under Vertex Data, Generate Data Layers, apply modifier. Covers the "Nearest Face Interpolated" mapping that works best for organic-to-organic transfer.
4. [Blender Studio — Bonus Tips: Weight Painting (problem joints)](https://studio.blender.org/training/weight-painting/5efe0f9e1f1b72e90e57ac81/) — 2023 — explicit cleanup patterns for shoulders, hips, elbows, knees (the four joints that always fail under auto-weight).
5. [Kiel Figgins — Painting Weights and Skinning: A Straightforward Approach](https://www.3dfiggins.com/writeups/paintingWeights/) — industry-vet reference on the "rough bind → fix hot spots → iterate in pose" methodology, mirror-symmetric workflow.
6. [CGCookie / Engeenee — Automatic Weights in Blender](https://engeenee.com/guides/models/body/automatic/) — 2024 — covers the "mesh changes shape on parent" gotcha (someone rotated bones in Pose Mode and forgot to Clear User Transforms).
7. [Polycount thread — auto-weights dropouts on deep concavities](https://polycount.com/discussion/217476/blender-parenting-armature-with-auto-weights-is-just-not-right) — community thread cataloguing why auto-weights fails on armpit, crotch, neck base (floating islands + bone heat tunneling).
8. *Internal:* `demo/fps_humanoid/assets/humanoid.skeleton.xml` — ground truth for the 19-bone target skeleton (positions, parent chain).
9. *Internal:* `third_party/base_humanoid/male/base_male.blend` — MPFB2 `game_engine` rig with 53 bones already attached, weights already painted by MPFB (`import_weights=True` in `add_builtin_rig`). This is our **source** armature; we reuse its weights where bone names overlap.

## MPFB `game_engine` → OdysseyEngine 19-bone map

Verified by reading `rig.game_engine.json` (53 keys) on 2026-04-21.

| OdysseyEngine bone (19) | MPFB game_engine source bone(s) | Notes |
| --- | --- | --- |
| `root` | `pelvis` | root of the engine rig is at Y=0.95 ≈ pelvis |
| `spine` | `spine_01` | |
| `chest` | `spine_02` + `spine_03` merged | also collect `clavicle_l` + `clavicle_r` weight into `chest` (engine has no clavicle bone) |
| `neck` | `neck_01` | |
| `head` | `head` | |
| `shoulder_l` / `shoulder_r` | **synthesized** — take `clavicle_{l,r}` weights and split between `chest` and `shoulder_{l,r}` | engine's `shoulder_{l,r}` is a 5cm stub off `chest`; weights live here for fine deformation near the upper-arm socket |
| `upper_arm_l` / `upper_arm_r` | `upperarm_{l,r}` | |
| `lower_arm_l` / `lower_arm_r` | `lowerarm_{l,r}` | |
| `hand_l` / `hand_r` | `hand_{l,r}` + all finger bones (`thumb_*`, `index_*`, `middle_*`, `ring_*`, `pinky_*`) | fingers collapse into `hand_{l,r}` — the 19-bone engine rig has no finger articulation |
| `upper_leg_l` / `upper_leg_r` | `thigh_{l,r}` | |
| `lower_leg_l` / `lower_leg_r` | `calf_{l,r}` | |
| `foot_l` / `foot_r` | `foot_{l,r}` + `ball_{l,r}` | |
| (dropped: `Root`) | MPFB's top-level `Root` is a zero-deformation control bone; discard. Engine's `root` maps to `pelvis`. |

34 of 53 MPFB bones are finger segments + ball-of-foot + clavicle that collapse into their parent. The remaining 19 map 1:1 (with one tree of spine bones merging to chest).

## Two workflows — pick one

### Workflow A: PROJECT the mesh onto the 19-bone engine armature and ARMATURE_AUTO skin it

Fastest. Use when the mesh is already scaled and feet-planted (which ours are — see `scale_to_engine_skeleton.md`). Drawback: auto-weights will try to invent bone-heat for the missing clavicle / ball / finger segments and leak weight into neighbours near those joints.

### Workflow B: WEIGHT-TRANSFER from the MPFB game_engine rig via Data Transfer modifier, then **rename** bones to match the 19-bone engine rig

Higher quality. Reuses MPFB's pre-painted weights (which MPFB ships via `weights.game_engine.json`) and avoids the concavity-auto-weight pathology at armpits/crotch. Requires a bone-rename / vertex-group-merge pass.

**Our default: Workflow B.** It preserves the painted-artist weights that MPFB ships and avoids the auto-weight dropouts at concavities.

## Consensus ordered steps (Workflow B)

1. **Open the scaled base mesh**: `bpy.ops.wm.open_mainfile(filepath="third_party/base_humanoid/<gender>/base_<gender>.blend")`. It already has the MPFB 53-bone `game_engine` armature plus painted weights attached as vertex groups.
2. **Remove the MPFB armature** from the scene — keep only its vertex-group data on the mesh. The vertex groups survive the armature deletion because they're stored on the mesh's `ob.vertex_groups` collection, independent of any armature:
   - `bpy.data.objects.remove(bpy.data.objects["base_<gender>.rig"], do_unlink=True)`
3. **Import the OdysseyEngine 19-bone armature** via a custom `bpy` builder that reads `humanoid.skeleton.xml` and constructs edit-bones at the specified positions. See `scripts/build_engine_armature.py` below.
4. **Bone-rename pass** — merge the 53 MPFB vertex groups into 19 engine-matching names using the mapping table above. This is a pure vertex-group rename + merge, no armature interaction required:
   ```python
   # Rename and merge vertex groups.
   merges = [
     (["pelvis"],                     "root"),
     (["spine_01"],                   "spine"),
     (["spine_02", "spine_03", "clavicle_l", "clavicle_r"], "chest"),
     (["neck_01"],                    "neck"),
     (["head"],                       "head"),
     # shoulder stubs get a fraction of the clavicle weight, handled specially below
     (["upperarm_l"],                 "upper_arm_l"),
     (["upperarm_r"],                 "upper_arm_r"),
     (["lowerarm_l"],                 "lower_arm_l"),
     (["lowerarm_r"],                 "lower_arm_r"),
     (["hand_l", "thumb_01_l","thumb_02_l","thumb_03_l",
                 "index_01_l","index_02_l","index_03_l",
                 "middle_01_l","middle_02_l","middle_03_l",
                 "ring_01_l","ring_02_l","ring_03_l",
                 "pinky_01_l","pinky_02_l","pinky_03_l"], "hand_l"),
     (["hand_r", "thumb_01_r","thumb_02_r","thumb_03_r",
                 "index_01_r","index_02_r","index_03_r",
                 "middle_01_r","middle_02_r","middle_03_r",
                 "ring_01_r","ring_02_r","ring_03_r",
                 "pinky_01_r","pinky_02_r","pinky_03_r"], "hand_r"),
     (["thigh_l"],                    "upper_leg_l"),
     (["thigh_r"],                    "upper_leg_r"),
     (["calf_l"],                     "lower_leg_l"),
     (["calf_r"],                     "lower_leg_r"),
     (["foot_l", "ball_l"],           "foot_l"),
     (["foot_r", "ball_r"],           "foot_r"),
   ]
   # For each (sources, dest), sum source weights into dest for each vertex, then remove sources.
   ```
   Implementation note: iterate `mesh.data.vertices` and for each vertex, sum the per-bone weights from the source groups, then write to the dest group via `dest_group.add([v.index], summed_weight, 'REPLACE')`. Delete source groups after.

5. **Spawn the engine armature** (if not already from step 3), name it `base_<gender>.engine_rig`.
6. **Select the mesh, shift-select the engine armature, Ctrl+P → Armature Deform (no auto weights!)**: `bpy.ops.object.parent_set(type='ARMATURE_NAME')`. This just parents the mesh to the armature and pre-creates the armature modifier; it does NOT overwrite the vertex-group weights — it assigns them by name.
7. **Clean up shoulder stubs**. The engine skeleton has `shoulder_l` + `shoulder_r` as 5-cm stubs off `chest`. MPFB has no direct equivalent (its `clavicle_{l,r}` already merged into `chest` in step 4). Manually add empty vertex groups `shoulder_l`, `shoulder_r`, then weight-blend a 30% portion of the `chest` weight within a 10-cm radius of the shoulder stub head toward `shoulder_{l,r}` via a proximity script (or just leave them empty — the upper-arm bone does all the deformation anyway).
8. **Validate**: enter Pose Mode on the engine armature, rotate `upper_arm_r` by 45°. The mesh should follow cleanly. If not, inspect the vertex groups (N panel → Object Data → Vertex Groups) and verify the 19 expected group names are all present.
9. **Save the result**: `bpy.ops.wm.save_as_mainfile(filepath="third_party/base_humanoid/<gender>/base_<gender>_skinned.blend")`.
10. **Export .obj** for engine descriptors (optional — the runtime still uses primitive-only rendering):
    ```python
    bpy.ops.wm.obj_export(
        filepath="third_party/base_humanoid/<gender>/base_<gender>_skinned.obj",
        export_selected_objects=True,
        export_uv=True, export_normals=True, apply_modifiers=True,
        forward_axis="NEGATIVE_Z", up_axis="Y",
    )
    ```

## Consensus ordered steps (Workflow A) — fallback

If weight-rename is fragile, fall back to pure auto-weights against the 19-bone armature:

1. Open `base_<gender>.blend`. Delete MPFB armature.
2. Build engine 19-bone armature from `humanoid.skeleton.xml`.
3. Select mesh, shift-select armature, `bpy.ops.object.parent_set(type='ARMATURE_AUTO')`. Blender will compute bone-heat weights for all 19 bones.
4. Inspect shoulders, hips, elbows, knees, neck base, crotch. Expect failure at 2+ of those joints; fix with weight paint.
5. Save.

Auto-weights is noisy for MPFB topology — MPFB's mesh has internal "helpers" (teeth, tongue, eyes, hair guides) which bone-heat may tunnel across, producing floating weights. Workflow B avoids this entirely.

## Bone-name consistency for retargeting

The engine `.anim.xml` format references bones by name (`<track bone="upper_arm_l">`). If your vertex groups and armature bones are named exactly as in `humanoid.skeleton.xml`, the existing `walk_cycle.anim.xml` drives the mesh directly. Retargeting is "free" via string match. This is why Workflow B's rename step is load-bearing — MPFB's `upperarm_l` (no underscore) must become engine's `upper_arm_l` (with underscore) or the anim won't bind.

## Blender 4.2 `bpy` equivalents

| Step | `bpy` call |
| --- | --- |
| 1 | `bpy.ops.wm.open_mainfile(filepath=SRC)` |
| 2 | `bpy.data.objects.remove(armature_obj, do_unlink=True)` |
| 3 | `armature_data = bpy.data.armatures.new("base_<g>.engine"); arm_obj = bpy.data.objects.new("base_<g>.engine_rig", armature_data); scene.collection.objects.link(arm_obj); bpy.context.view_layer.objects.active = arm_obj; bpy.ops.object.mode_set(mode='EDIT'); for b in bones: eb = armature_data.edit_bones.new(b.name); eb.head = b.world_head; eb.tail = b.world_head + vec(0, b.length, 0); ...` |
| 4 | `for src_names, dst in merges: merge_vertex_groups(mesh, src_names, dst)` (custom python) |
| 6 | With mesh active and armature selected-last: `bpy.ops.object.parent_set(type='ARMATURE_NAME')` |
| 7 | Proximity weight blend via `bmesh` + `kdtree.KDTree` on shoulder bone head |
| 8 | Unit-test: rotate `upper_arm_r`'s pose bone 45° about local X; check no vertex moves > 5cm away from its expected shoulder-influenced position. |
| 9 | `bpy.ops.wm.save_as_mainfile(filepath=DST)` |

## Gotchas

- **MPFB's rig has a top-level `Root` bone** (capital R) distinct from `pelvis`. Do NOT map `Root` to engine's `root` — map `pelvis` to `root`. MPFB's `Root` is a zero-deform control; discard it.
- **Finger weights collapse to `hand_{l,r}`** — this means posing fingers on the engine rig is impossible. Accept as "half decent" bar. If finger articulation is ever needed, add 4 bones per hand (thumb + 3 fingers) to the engine skeleton — council decision.
- **Clavicle collapses to `chest`** — this means the shoulder cannot shrug. The `shoulder_{l,r}` stubs in the engine rig are rotational-only (no translation authored on the clavicle in engine's walk anim). For most walks/idles this is fine.
- **Eye/teeth/tongue vertex groups** from MPFB — MPFB's body mesh sometimes has `mhjaw`, `mheyes` etc. groups if `extra_vertex_groups=True`. These are safe to leave in place (no bone binds to them) but optionally clean them up for tidiness.
- **The `import_weights=True` flag** on `HumanService.add_builtin_rig(basemesh, "game_engine", import_weights=True)` loads the weights from `weights.game_engine.json` — confirm those vertex groups are actually on your mesh (`len(mesh.vertex_groups) == 53`) before trusting the rename pass. If they're empty, fall back to Workflow A.
- **Auto-weights expects a closed manifold** — the MPFB base has open mouth interior. If you ever switch to Workflow A and fail, run `bpy.ops.mesh.fill_holes()` on a duplicate first.
- **Pose-mode leakage**: if you ever rotated the MPFB armature in pose mode, rest-position weight-painting will be incorrect. Always `bpy.ops.pose.transforms_clear()` + `bpy.ops.object.mode_set(mode='OBJECT')` before skinning.
- **Subtle: armature-modifier ordering** matters if the mesh has a solidify or shrinkwrap modifier from a previous pass. `Armature` must be above (applied last) to drive deformation. Reorder with `bpy.ops.object.modifier_move_to_index`.
- **Weight normalization**: after merging groups, re-normalize so all vertex weights sum to 1.0 per vertex. Blender: `bpy.ops.object.vertex_group_normalize_all(lock_active=False)`.

## Validation checklist

Before marking a mesh `skinned`:

- [ ] Vertex group count == 19 (exact), names == 19 engine-skeleton bone names
- [ ] All 19 vertex groups non-empty (at least 1 vertex with weight > 0)
- [ ] Sum of weights per vertex in [0.999, 1.001] (normalization)
- [ ] Rotate each of the 19 bones in Pose Mode; no vertices disappear to origin, no mesh tearing visible
- [ ] Armature modifier present on mesh, pointing at the engine armature
- [ ] Driving with `walk_cycle.anim.xml` (by retargeting or re-importing as .blend action) produces legible walking pose

## Post-task update log

(Appended by sessions that use this skill. Newest on top.)

- 2026-04-21 — skill authored. Mapping table derived by reading `rig.game_engine.json` directly (53 bone keys, UE4-Mannequin naming). Workflow B (weight-rename + Data Transfer if needed) preferred over Workflow A (pure auto-weights).
