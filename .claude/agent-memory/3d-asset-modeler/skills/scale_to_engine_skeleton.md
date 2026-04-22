---
skill: scale_to_engine_skeleton
difficulty: beginner
prerequisites: [generate_base_male or generate_base_female produced a .blend]
status: complete — executed 2026-04-21
blender target: 4.2 LTS
---

## Goal

Normalize a humanoid base mesh + rig to the OdysseyEngine 19-bone humanoid convention: total height ~1.8-1.9 m, feet planted at Z=0. Uniform-scale the armature (mesh follows because it's parented), apply scale into geometry + bone rest positions, translate so feet touch ground, apply translation. Result: `.blend` + `.obj` with no outstanding transforms.

## Why this is its own skill

- MPFB bases come out ~1.69 m (not 1.80). You cannot fix this via the `height` macro slider — that only shifts a few cm.
- Engine conventions (root at Y=0.95, feet at Y=0, total ~1.8 m) must be hit explicitly.
- Applying transforms before export is mandatory or the engine's tiny_obj loader sees rescaled vertices but the rig's bones carry the pre-scale local positions.

## Sources

1. [CGDive — "correct scale for game engines"](https://cgdive.com/) — general best-practice for game-ready character export. Standard Unreal/Unity advice: apply all transforms, 1 Blender unit = 1 metre.
2. [Blender Stack Exchange — "Apply transforms" answers (various)](https://blender.stackexchange.com/questions/tagged/transforms) — practical ordering: select armature + mesh together, apply scale. If you apply to mesh only, bone rest positions become wrong.
3. [Blender Manual 4.2 — Object → Apply](https://docs.blender.org/manual/en/latest/scene_layout/object/editing/apply.html) — canonical docs for `object.transform_apply`.
4. [Unreal Engine Docs — Humanoid character scale conventions](https://docs.unrealengine.com/) — 1 uu = 1 cm; humanoid height 180 cm → 1.80 m in Blender.
5. Internal probe 2026-04-21 of `demo/fps_humanoid/assets/humanoid.skeleton.xml` — confirmed: root position `0 0.95 0`, feet at `y=0`, top of head ~1.65 m from bone positions (mesh can extend higher to 1.80-1.91 m for head cap).

## Consensus ordered steps

```python
import bpy
TARGET_HEIGHT = 1.80  # metres

bpy.ops.wm.open_mainfile(filepath="base_male.blend")
basemesh = bpy.data.objects["base_male"]
armature = bpy.data.objects["base_male.rig"]

# 1. Compute current world-space height (with shape-keys evaluated).
bpy.context.view_layer.update()
deps = bpy.context.evaluated_depsgraph_get()
bm_eval = basemesh.evaluated_get(deps)
coords = [basemesh.matrix_world @ v.co for v in bm_eval.data.vertices]
pre_height = max(c.z for c in coords) - min(c.z for c in coords)
scale = TARGET_HEIGHT / pre_height

# 2. Uniform-scale armature + mesh, then apply.
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True); basemesh.select_set(True)
bpy.context.view_layer.objects.active = armature
armature.scale = (scale,)*3
basemesh.scale = (scale,)*3
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

# 3. Translate so feet plant at Z=0.
bpy.context.view_layer.update()
coords = [basemesh.matrix_world @ v.co for v in basemesh.data.vertices]
armature.location.z += -min(c.z for c in coords)
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

# 4. Re-save .blend and re-export .obj.
bpy.ops.wm.save_as_mainfile(filepath="base_male.blend")
bpy.ops.object.select_all(action="DESELECT")
basemesh.select_set(True); bpy.context.view_layer.objects.active = basemesh
bpy.ops.wm.obj_export(filepath="base_male.obj", export_selected_objects=True,
                      export_uv=True, export_normals=True, export_materials=False,
                      apply_modifiers=True, forward_axis="NEGATIVE_Z", up_axis="Y")
```

## Gotchas

- **Always select BOTH armature + mesh before `transform_apply(scale=True)`**. Applying to mesh only leaves the armature's bone rest positions in pre-scale coords — rig bones end up inside the body / outside the body.
- Set the **armature as active object** (`view_layer.objects.active = armature`) before apply — otherwise Blender warns "object has no parent" when mesh is active but armature is not.
- `min(c.z)` for the foot-planting step must use **post-scale** coords. If you read coords before applying scale, the shift is wrong.
- On MPFB bases with `mask_helpers=True`, the Mask modifier hides helper geometry at render time but NOT for vertex iteration. Helper verts still count in `basemesh.data.vertices`. Use `evaluated_get(depsgraph).data.vertices` if you want to respect the mask.
- Final height was 1.912 m (not exactly 1.80) in our run because the mesh head-cap extends above the top-of-head bone. Accept the 1.8-1.95 m range for heroic proportions; don't force-clamp.
- Do NOT rotate the armature to "face camera" here — that's a scene-composition decision, not a normalization step.

## Post-task update log

- 2026-04-21 — Session 9613f92d — Ran on both male and female bases. Both ended at 1.912 m (same scale factor 1.0622) because MPFB's internal height is identical pre-macro. Feet planted at 0. Scripts: `demo/showcase/assets/base_humanoid/scripts/scale_to_engine_skeleton.py`.
