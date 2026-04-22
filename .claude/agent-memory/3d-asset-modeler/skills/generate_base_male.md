---
skill: generate_base_male
difficulty: beginner
prerequisites: [install_mpfb2 — must be enabled in Blender 4.2+]
status: complete — executed end-to-end 2026-04-21
blender target: 4.2 LTS
---

## Goal

Generate a muscular, attractive CC0 male humanoid base mesh via MPFB2's Python services. Output a rigged `.blend` (game_engine rig), a triangulated `.obj`, license files, and stats JSON.

## Critical gotcha — the gender slider is INVERTED from the obvious reading

MPFB's `macro_detail_dict["gender"]` ranges 0.0 to 1.0 but:

- **0.0 → FEMALE** (high-weight = "female" target)
- **1.0 → MALE** (high-weight = "male" target)

Confirmed by inspecting `data/targets/macrodetails/macro.json`:

```json
"gender": {"label": "Gender", "parts": [{"high": "male", "highest": 1.01, "low": "female", "lowest": -0.01}]}
```

Setting `gender=0.0` when you want "male" produces a female mesh silently (all other sliders still apply, so you get a muscular female, not a muscular male). Always pass **`gender=1.0` for male, `gender=0.0` for female**.

## Sources

1. [GitHub — makehumancommunity/mpfb2](https://github.com/makehumancommunity/mpfb2) — 2026 — canonical source for the Python API. Read `src/mpfb/services/humanservice.py` directly.
2. [MakeHuman Community — Getting started](https://static.makehumancommunity.org/mpfb/docs/getting_started.html) — 2026 — "new human → from scratch" UI flow; operator `bpy.ops.mpfb.create_human()` is the UI-equivalent.
3. [MPFB Tutorial: Installation and Getting Started (YouTube)](https://www.youtube.com/watch?v=FNeiLDH_lnw) — 2026 — walks the slider set (gender, age, muscle, weight, proportions, height, cupsize, firmness, race).
4. [Blender Artists Community — Create Character Inside blender With Makehuman](https://blenderartists.org/t/create-character-inside-blender-with-makehuman-blender-add-on/1365257) — 2026 — practical how-to, includes macro slider rundown with screenshots.
5. Internal probe 2026-04-21 of `HumanService.create_human(...)` signature + `TargetService.get_default_macro_info_dict()` + `_interpolate_macro_components()` in the installed v2.0.15 build 20260421 source tree. This is the authoritative reference.

## Consensus ordered steps — headless script

```python
import bpy
from bl_ext.user_default.mpfb.services.humanservice import HumanService

bpy.ops.wm.read_factory_settings(use_empty=True)

macro = {
    "gender": 1.0,            # 1.0 = MALE; DO NOT flip
    "age": 0.30,              # young adult
    "muscle": 0.90,           # muscular
    "weight": 0.55,           # slight mass
    "proportions": 0.50,      # average
    "height": 0.55,           # slightly tall
    "cupsize": 0.5,           # ignored for male
    "firmness": 0.5,          # ignored for male
    "race": {"asian": 0.20, "caucasian": 0.60, "african": 0.20},
}
basemesh = HumanService.create_human(
    mask_helpers=True,
    detailed_helpers=False,
    extra_vertex_groups=True,
    feet_on_ground=True,
    scale=0.1,
    macro_detail_dict=macro,
)
basemesh.name = "base_male"

# Add game-engine rig.
armature = HumanService.add_builtin_rig(basemesh, "game_engine", import_weights=True)
armature.name = "base_male.rig"

# Apply location, leave scale for scale_to_engine_skeleton step.
bpy.context.view_layer.objects.active = basemesh
basemesh.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

# Export OBJ (Blender 4.2 API).
bpy.ops.wm.obj_export(
    filepath="base_male.obj",
    export_selected_objects=True,
    export_uv=True,
    export_normals=True,
    export_materials=False,
    apply_modifiers=True,
    forward_axis="NEGATIVE_Z",
    up_axis="Y",
)

bpy.ops.wm.save_as_mainfile(filepath="base_male.blend")
```

## Gotchas

- **Gender inversion** (see top). Always pass `gender=1.0` for male.
- `create_human` is a **service** call, not a `bpy.ops.*` operator. It returns the mesh object directly — capture the return value.
- Import path is `bl_ext.user_default.mpfb.services.humanservice`, **not** `mpfb.services.humanservice`. The extensions platform namespaces the module under `bl_ext.user_default`.
- Shape keys from macro targets are stored as shape keys on the mesh, **not baked into rest geometry**. `basemesh.data.vertices[0].co` returns the rest position (pre-deformation); the deformed position is at `basemesh.evaluated_get(depsgraph).data.vertices[0].co`. OBJ export with `apply_modifiers=True` bakes the shape keys into the export geometry automatically.
- `HumanService.create_human(...)` internally triggers `ObjectService.load_base_mesh(...)` which runs `bpy.ops.wm.obj_import(...)` to load `base.obj` — if there's a stdout warning `OBJ import of 'base.obj' took N ms`, that's normal.
- The `game_engine` rig (53 bones) is **not** the OdysseyEngine 19-bone skeleton. Retargeting is a separate skill (`retarget_rig.md`).
- Rig bone names (from `rig.game_engine.json`) use Unreal-style conventions: `pelvis`, `spine_01/02/03`, `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`, `thigh_l/r`, `calf_l/r`, `foot_l/r`, `head`, `neck_01`. Useful to know when writing `skin_base_to_skeleton.md` or `retarget_rig.md`.
- **MPFB body is ~1.69 m tall by default** at `scale=0.1`, not the documented 1.80. The `height=0.55` slider only nudges by a couple cm — do NOT rely on the height slider alone to hit 1.8 m. Use `scale_to_engine_skeleton.md` afterwards.

## Expected output stats (v2.0.15 build 20260421)

- verts = 19158
- polys = 18486 (100 % quads, 0 tris, 0 n-gons)
- bone count = 53 (game_engine rig)
- raw height (pre-scale): 1.6946 m; after scale_to_engine_skeleton: 1.912 m
- 15+ active shape keys after macro-apply (visible via `data.shape_keys.key_blocks`)

## Post-task update log

- 2026-04-21 — Session 9613f92d — First run used `gender=0.0` and produced a female mesh. Detected via render inspection and manual probe of shape-key names (`$fe` prefix). Re-ran with `gender=1.0` and got a correct muscular male. Updated the "gender inversion" gotcha to top of doc. Output: `third_party/base_humanoid/male/base_male.{blend,obj}`.
