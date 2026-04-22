---
skill: sculpt_armor_plate
difficulty: intermediate
prerequisites: [a rigged base mesh .blend from generate_base_{male,female} + scale_to_engine_skeleton]
status: complete — executed 2026-04-21 as proof-of-workflow
blender target: 4.2 LTS
---

## Goal

Non-interactive "sculpt" of a hard-surface armor plate onto a base humanoid, via the modifier stack: plane-add → subdivide → shrinkwrap → solidify → bevel → weighted-normal → shade-smooth + Etherealism material. Result attaches to the authoring `.blend` as a child object (`base_{gender}_chestplate`) so renders can show the base armored.

## Sources

1. [Grant Abbitt — "Sculpt armor with Blender" YouTube series](https://www.youtube.com/@grantabbitt) — 2023-2025 — canonical mask-extract + solidify workflow. Interactive; we approximate headlessly via shrinkwrap.
2. [FlippedNormals — "Hard-surface armor in Blender"](https://flippednormals.com/) — 2024 — modifier-stack recipe (subdivide + shrinkwrap + solidify + bevel + weighted-normal).
3. [Dikko — "Armor modeling tutorial"](https://www.youtube.com/@dikkoyt) — 2023 — shrinkwrap offset tuning; 1-2 cm standoff reads as "plate worn over body armor under-layer".
4. [Blender Manual 4.2 — Shrinkwrap modifier](https://docs.blender.org/manual/en/latest/modeling/modifiers/deform/shrinkwrap.html) — 2026 — PROJECT vs NEAREST_SURFACEPOINT semantics.
5. [YanSculpts — "Hard-surface sculpting fundamentals"](https://www.youtube.com/@YanSculpts) — 2023 — why weighted-normal matters for crisp facets on low-poly bevel.

## Consensus ordered steps — headless, modifier-stack based

```python
# Open the scaled base .blend first
import bpy

# 1. Compute chest band from the basemesh bounding box.
basemesh = bpy.data.objects["base_male"]  # or base_female
bpy.context.view_layer.update()
coords = [basemesh.matrix_world @ v.co for v in basemesh.data.vertices]
min_z, max_z = min(c.z for c in coords), max(c.z for c in coords)
chest_z_mid = min_z + 0.68 * (max_z - min_z)  # sternum-ish

# 2. Spawn a flat plane at the chest, 26 cm x 28 cm.
bpy.ops.mesh.primitive_plane_add(
    size=0.26,
    location=(0.0, -0.10, chest_z_mid),
    rotation=(1.5708, 0, 0),  # plane normal points -Y (outward front)
)
plate = bpy.context.active_object
plate.name = f"{basemesh.name}_chestplate"
plate.scale.z = 0.28 / 0.26  # non-square aspect
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

# 3. Subdivide to 8x8 grid for shrinkwrap fidelity.
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.subdivide(number_cuts=6)
bpy.ops.object.mode_set(mode="OBJECT")

# 4. Shrinkwrap to the body.
sw = plate.modifiers.new("ArmorShrinkwrap", "SHRINKWRAP")
sw.target = basemesh
sw.wrap_method = "NEAREST_SURFACEPOINT"
sw.offset = 0.018  # 1.8 cm standoff

# 5. Solidify — give the plate thickness.
sol = plate.modifiers.new("ArmorSolidify", "SOLIDIFY")
sol.thickness = 0.012
sol.offset = 1.0  # all thickness goes outward
sol.use_even_offset = True
sol.use_quality_normals = True

# 6. Bevel — ease the rim so it catches rim light.
bev = plate.modifiers.new("ArmorBevel", "BEVEL")
bev.width = 0.003
bev.segments = 2
bev.limit_method = "ANGLE"
bev.angle_limit = 0.52  # ~30°

# 7. Weighted normal — crisp facets.
wn = plate.modifiers.new("ArmorWeightedNormal", "WEIGHTED_NORMAL")
wn.weight = 100

# 8. Shade smooth.
bpy.ops.object.shade_smooth()

# 9. Matte-dark Etherealism material (almost-black tint, low metallic).
# See render_hero_shot_etherealism.md for the full palette rationale.
```

## Gotchas

- **NEAREST_SURFACEPOINT wraps around**. With a small plate (< body-width) it's fine. A larger plate (> 30 cm wide) wraps onto the shoulders / arms, producing a poncho-shaped plate. Use PROJECT with a directional axis for larger plates.
- **PROJECT shrinkwrap needs `project_limit`** — without it, rays that miss the body project to infinity, leaving those plate verts at the plate's original position. Produces 3D "shards" floating off to the side.
- **PROJECT + `cull_face="BACK"`** prevents the ray from punching through to the spine/back — essential when doing frontal chest plates.
- **PROJECT floaters: post-apply trim pass**. Even with `project_limit` set, verts whose ray misses the body (outer edges outside the body silhouette) stay at the plate's spawn depth. The cleanest fix is a **post-modifier-apply trim pass**: after applying the shrinkwrap modifier, enter edit mode on the mesh, select verts still at world-Y near FRONT_Y (within ~2cm of the spawn Y), and delete them. This removes "floaters" without requiring the plate to be pre-sized exactly to body silhouette.
- **Chevron V survives shrinkwrap only if drop > shrinkwrap amplitude.** If you pre-displace bottom verts down by 2cm but shrinkwrap snaps them to the chest curvature (which locally pulls them up ~3cm), the V flattens. Use `chevron_drop >= 4cm` for a visible V after shrinkwrap.
- **Sternum/chevron stacking** — use staggered `offset` values per piece (e.g. 0.012 / 0.022 / 0.018) so adjacent layers visibly overlap. Same-offset plates merge into one flat shape.
- **Z landmarks: keep chest Z ≤ 0.73 × body_height**. Above that, chevron plate tops stretch into the clavicle / neck during shrinkwrap. Sternum ridge: Z = 0.66-0.70 × body_height. Upper chevron top max: 0.72.
- **Base mesh material defaults to white**. MPFB-generated bases ship with no assigned material → they render pure white under Etherealism lighting. Assign a matte-dark "undersuit" material (albedo 0.02, roughness 0.95) to the base mesh before rendering so the armor reads as a distinct piece.
- **Armor metallic 0.25 roughness 0.65** reads better than the Berserk-Halo default (0.15 / 0.78) when the armor is small relative to the frame — the extra specular edge highlights help the plates pop against the body. For full-size Berserk-Halo armor at 30m silhouette, stick with the original values.
- **Front-view rim lighting needs dual rim**. A single back-left rim at `(-1.8, +2.2, +2.8)` is great for 3/4 and side views but leaves the armor front under-lit. Add a secondary rim at `(+1.6, +1.8, +2.4)` (~1200 W) to light the opposite flank.
- **Female vs male topology differs materially.** A script that produces a clean plate on a male base can produce broken geometry on the female because breast surface normals pull the nearest-surface projection into concavities. Validate on both; expect variant tuning per gender.
- **Subdivide resolution**: `number_cuts=5` gives 6×6 grid per side (72 verts); `number_cuts=6` gives 8×8 (128 verts). For chevron plates with chevron_drop edits, `number_cuts=5` provides enough verts that the V survives the shrinkwrap without excessive poly-count.
- **Solidify offset=1.0** puts all thickness outward; `offset=0.0` is symmetric (half inward, half outward); `offset=-1.0` pushes inward (plate ends up INSIDE the body). For visible armor, always 1.0.
- **Shade smooth without weighted-normal** produces visible smoothing artifacts around the bevels. Always chain weighted-normal after bevel, and apply shade-smooth last.
- **Apply modifiers** only when exporting; keep them in the stack on the authoring .blend so you can re-tune. EXCEPTION: if you need a `trim_floaters` post-pass, the trim must happen after apply, so authoring .blend will have modifiers baked in (acceptable for one-shot assets).
- **Plate authoring order: SCALE then ROTATE (not rotate then scale).** `primitive_plane_add(rotation=(90°,0,0))` spawns a plane with its rotation-euler set but mesh verts still in local XY. If you then set `plate.scale.z = height/width` and call `transform_apply(scale=True, rotation=True)`, the baked scale is applied along OBJECT Z, which (after the 90° X-rotation) corresponds to what was originally local Y — and the flat plane's local Y is already 0 for all verts. Result: the "scale up" silently multiplies zero and the plate stays its original small size. Silent data loss. Correct order: spawn plane flat (no rotation), set `plate.scale.y = height/width` while plane is flat, `transform_apply(scale=True)`, THEN `plate.rotation_euler = (±90°, 0, 0)` and `transform_apply(rotation=True)`.
- **Mirrored symmetric plates: author as separate L/R halves, not one wide plate with mid-line pinch.** A single 30cm plate at X=0 with PROJECT or NEAREST_SURFACEPOINT routinely produces asymmetric results because the body's pec/shoulder geometry is slightly asymmetric per-side (MPFB generation variance) or the camera is at a non-symmetric angle. Clean fix: author `plate_l` at x_center=-halfwidth/2 and `plate_r` at x_center=+halfwidth/2, mirror-symmetric inputs otherwise. This also makes mirrored roll (left gets +roll, right gets -roll) natural.
- **Chevron V-shape is ROLL about Y, not YAW about Z.** Rolling a plate (in XZ plane facing -Y) about Y tilts the plate so one X-edge rises and the other drops → visual V sweep. Yawing about Z rotates the plate in horizontal plane → plate wraps around torso (useful, but NOT what "chevron" reads as visually). Use ±18° roll for each half with mirrored signs.
- **PROJECT shrinkwrap: use `cull_face="OFF"` by default.** FRONT/BACK semantics in Blender's shrinkwrap PROJECT mode are ambiguous (conflicting docs, version-dependent) and easy to get backward. `cull_face="FRONT"` with rays going -Y successfully culled the whole back surface of a male base mesh in one test (zero verts landed on body). `cull_face="OFF"` is robust: ray hits first surface regardless of normal orientation, which is what you want 95% of the time. If you need to avoid shooting through to the FAR side of a body (e.g. chest plate accidentally wrapping onto back), control that with a tight `project_limit` and a well-placed spawn location, NOT with cull_face.
- **`project_limit` must exceed the spawn-to-body distance by margin.** Upper-back Y at shoulder-blade height is only ~+0.05 on a 1.9m male base, while sides of the upper back (~shoulder) can be Y ≈ +0.15. A plate spawned at Y=+0.30 with project_limit=0.20 reaches Y=+0.10 max — misses the upper spine. Rule of thumb: project_limit = spawn_to_body_distance × 1.5.
- **`trim_floaters` needs two criteria, not one.** (a) Near-spawn (delete verts at Y within `margin` of spawn_y) catches verts whose rays missed entirely. (b) Absolute cutoff (delete verts on wrong side of max-legit-body-Y + offset + solidify) catches verts whose rays hit `project_limit` boundary and stopped partway — neither at spawn nor on body. Both are needed; a margin-only trim leaves partial-miss floaters. Example for male base: front plates use cutoff Y > -0.25 (body chest ≈ -0.22); back plates use cutoff Y < +0.17 (body back ≈ +0.12).
- **Dark undersuit at 0.08 albedo still renders gray under Etherealism.** Spec-style values of (0.08, 0.08, 0.10) look near-black as swatches but Filmic Medium-High-Contrast + a 2000W rim light drives them to ~40% gray at the silhouette edge. For a clearly dark undersuit that reads "matte dark bodysuit" against a metallic-0.35 armor at 0.22 albedo, drop to (0.030, 0.030, 0.045), roughness 0.90, and reduce "Specular IOR Level" to 0.10 to cut the rim-induced specular bloom.
- **Light rigs are directional — bright rims behind the subject make back views bright.** The Etherealism 3-point rig places rim lights behind-left and behind-right of the subject (for front-view "halo" effect). When a camera is positioned BEHIND the subject (back-view render), those rims become DIRECT key lights hitting the subject from the camera's side, producing an overall brighter back view. Not a material bug, a lighting-per-view trade-off. Options: (a) accept the brighter back view; (b) add a view-dependent light switch (expensive); (c) reposition rims to the sides (compromises front-view hero shot).

## Post-task update log

- 2026-04-21 — Session 9613f92d — Ran on male and female bases. Male plate read cleanly as a chest-fitted rectangular module with subtle side flanges (shrinkwrap caught the pec edges). Female plate broke: the nearest-surface projection caught breast-adjacent concavities and produced "floating 3D chunks" on the three-quarter view. Documented in the gotcha section. Male delivery: acceptable proof-of-workflow. Female delivery: visible failure mode, retained for honest documentation.
- 2026-04-21 — Extended to MULTI-PIECE TORSO ARMOR on male base (sternum ridge + 3 chevron plates + 5 ribbed abdominal segments). New skill outputs: `sculpt_torso_armor_male.py`, `torso_armor.blend`, `torso_armor.obj` (1794 v / 3332 tri). 9 pieces assembled. Four iterations required to get a clean read:
  1. First pass used `NEAREST_SURFACEPOINT` with 34cm-wide chevrons → wraparound onto shoulders, armor appeared at neck height. Lesson: chest-plate widths > 0.28m wrap shoulders even on male.
  2. Second pass lowered Z landmarks and narrowed plates → good placement, but chevrons floated off the side with `PROJECT` shrinkwrap (rays outside body silhouette → verts stay at spawn depth).
  3. Third pass: `PROJECT` + `cull_face="BACK"` + **`trim_floaters(y_threshold=-0.23)`** post-modifier-apply trim. Added to skill gotchas. Solved the floaters, kept wide plates.
  4. Fourth pass: armor metallic bumped 0.15→0.25, roughness 0.78→0.65 for specular pop; added a 2nd rim light (upper-right, 1200 W) for front-view readability. Body undersuit material (0.02 albedo, 0.95 rough) replaces MPFB's default white so the armor reads as distinct.
- **Reads at 30m silhouette as: "layered brutal sci-fi carapace"** — per-plate stacking visible in 3/4 view, sternum ridge + chevron V visible in front view, ribbed abdominal taper visible in side view.
- 2026-04-21 — ITER-2 fix pass (visual-QA review rejected first pass). 5 defects fixed: (1) centered on X=0 via sternum-first then mirrored L/R chevron halves (2) back carapace added: 7 vertebrae + 2 scapula + 1 lumbar (3) body undersuit darkened from 0.08 spec to 0.030 tuned (4) coverage expanded, chevrons 38/34/30cm widths, ribs 28→20cm taper, sternum runs full chest height (5) chevrons use Y-axis ROLL (not Z-yaw) for visible V slant. Three bugs discovered during fix pass, all documented in gotchas: scale-before-rotate plate authoring order, cull_face=OFF default, trim_floaters two-criterion design. Final asset: 22 pieces, 4132 verts / 7906 tris. Reads as coherent carapace from front/3q/side at 30m silhouette. Back view slightly compromised by Etherealism rim-light placement (rims behind subject become direct lights on back-camera); accepted as a view-dependent trade-off.
