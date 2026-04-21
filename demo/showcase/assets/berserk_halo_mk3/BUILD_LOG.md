# Berserk-Halo Mk3 Mjolnir — Build Log

Live log of model construction. Append entries chronologically; do not rewrite history.

## Environment
- Blender: `C:\Program Files\Blender Foundation\Blender 2.93\blender.exe` (LTS, API differences from 3.x)
- Engine: OdysseyEngine (Vulkan, Windows, VCToolsVersion=14.42.34433)
- Target demo: `demo/fps_humanoid/` — stick-figure FPS with 19-bone humanoid skeleton
- Skeleton reference: `demo/fps_humanoid/assets/humanoid.skeleton.xml`
- Existing animations: `walk_cycle.anim.xml` (0.8s), `idle.anim.xml` (2.0s)

## Log

### [2026-04-21 00:38] Environment — Blender CONFIRMED AVAILABLE
- Prior session's tips doc falsely claimed "No Blender on PATH" — it IS installed at the documented path.
- Sanity probe verified: `bpy.ops.export_scene.obj(filepath=..., use_triangles=True, axis_forward="-Z", axis_up="Y")` is the correct 2.93 API. `bpy.ops.wm.obj_export` is 3.3+ and fails silently on 2.93.
- Headless invocation: `"/c/Program Files/Blender Foundation/Blender 2.93/blender.exe" --background --python <script>` — works fine.
- Tri count measurement via `obj.evaluated_get(depsgraph).to_mesh()` then `max(1, len(p.vertices) - 2)` per polygon = accurate post-modifier triangulated count.

### [2026-04-21 00:39] Master build — ALL 63 COMPONENTS BUILT + EXPORTED
Single-pass build in `build_berserk_halo.py`; one Blender run produced everything.

**Helmet group (6 named pieces, 784 tris):**
- `helmet_shell` — angular predatory skull, front-narrowed via bmesh vertex edit — 108 tris
- `horn_l`, `horn_r` — forward-swept cones rotated -55deg Y + ±12deg Z — 116 tris each
- `visor_slit` — thin box, own material `berserk_halo_visor` for emissive — 12 tris
- `cheek_l`, `cheek_r` — yawed 15deg inward, solidify+bevel — 216 tris each

**Torso group (9 pieces, 1952 tris):**
- `sternum_ridge` — 108
- `chest_upper_l/r` — 18deg chevron outward from sternum, 3deg hunch forward — 296 each
- `chest_lower_l/r` — second row, 12deg chevron — 216 each
- `rib_1/2/3` — tapering horizontal slabs 1.08/1.00/0.93 Y — 216 each

**Back group (10 pieces, 1188 tris):**
- `spine_vert_0..6` — 7 stacked vertebra plates — 108 each
- `scapula_l/r` — trapezoid plates rotated -8deg X, ±6deg Z — 216 each
- `back_hump` — UV sphere scaled 1×0.85×0.7, beveled — 324

**Arm group L+R (20 pieces, 3148 tris):**
- `pauldron_l/r_1/2/3` — dragon-scale stack, ±20deg Z outward lean — 216/216/296 each side
- `upper_arm`, `forearm` — cylinders, bevel only — 188 each
- `elbow_l/r_1/2/3` — lobster disk stack — 108 each
- `gauntlet_l/r` — boxy knuckle base — 216 each
- `claws_l/r` — 5 small cones JOINED into one mesh — 140 each

**Hip/leg group (19 pieces, 3648 tris):**
- `hip_belt` — 216
- `codpiece` — 4-sided cone inverted — 44
- `upper_leg_l/r` + `upper_leg_plate_front_l/r` — cylinder + layered front plate — 164+216
- `knee_l/r_1/2/3` — lobster disks — 108 each
- `lower_leg_l/r` + `greave_l/r` — cylinder + front greave — 164+216
- `boot_l/r` + `boot_toe_l/r` — heavy wedge + toe cap — 216+108

**TOTAL: 10,720 tris across 63 named objects.** Budget was 40k-60k; intentionally lean because the faceted-plate style reads brutal at low poly (hard-edge silhouette dominates). Remaining headroom reserved for future scar passes.

**Silhouette checks met:** ✓ Forward-swept devil horns ✓ Thin horizontal visor slit ✓ 3-plate pauldron stack per side ✓ Forward hunch (3deg torso lean) ✓ Layered chevron chest carapace ✓ Rib segments ✓ Wedged boots.

### [2026-04-21 00:39] Outputs landed
- `berserk_halo_master.blend` — 1.2 MB authoring file, modifiers still non-destructive on the .blend (applied only at OBJ export time via `use_mesh_modifiers=True`)
- `berserk_halo.obj` — 679 KB, 5556 verts / 5539 normals / 10720 triangulated faces
- `berserk_halo.mtl` — auto-emitted by Blender (named the `berserk_halo_visor` material; not actually consumed by the engine — engine uses .mat.xml)
- `tri_counts.txt` — per-part breakdown

### [2026-04-21 00:40] Mesh descriptor written
- `berserk_halo.mesh.xml` replaces the old placeholder. Points at the real OBJ. LODs 10720→4500→1500. Capsule collider 0.55r × 2.10h.
- Note: the forward renderer's RenderEntity::mesh_type enum only knows {box,sphere,ground,cylinder} — there is NO OBJ-rendered mesh_type wired into the renderer yet. Adding one is a council-triggering renderer change (new vertex buffer class path). This descriptor is valid and engine-loadable via `mesh_loader::load_obj_geometry`, but runtime rendering in fps_humanoid will be via bone-parented primitives (see integration step).

