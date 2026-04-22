---
skill: generate_base_female
difficulty: beginner
prerequisites: [install_mpfb2 — must be enabled in Blender 4.2+]
status: complete — executed end-to-end 2026-04-21
blender target: 4.2 LTS
---

## Goal

Generate a shapely, attractive CC0 female humanoid base mesh via MPFB2's Python services. Output a rigged `.blend` (game_engine_with_breast rig — adds two breast bones), `.obj`, license, stats JSON.

## Gender slider (CRITICAL — same gotcha as generate_base_male)

MPFB's `macro_detail_dict["gender"]` is 0.0 = **FEMALE**, 1.0 = **MALE**. For a female, pass **`gender=0.0`**. See `generate_base_male.md` for the full rationale (confirmed via `data/targets/macrodetails/macro.json`).

## Sources

1. [GitHub — makehumancommunity/mpfb2](https://github.com/makehumancommunity/mpfb2) — 2026 — `HumanService.create_human(...)` signature.
2. [MakeHuman Community — Getting started](https://static.makehumancommunity.org/mpfb/docs/getting_started.html) — 2026 — UI flow.
3. [MPFB Tutorial: Installation and Getting Started (YouTube)](https://www.youtube.com/watch?v=FNeiLDH_lnw) — 2026 — slider walkthrough.
4. [Blender Artists Community — MakeHuman Blender plugin thread](https://blenderartists.org/t/create-character-inside-blender-with-makehuman-blender-add-on/1365257) — 2026.
5. Internal probe of `HumanService.create_human`, `TargetService.get_default_macro_info_dict`, and `rig.game_engine_with_breast.json` in the MPFB v2.0.15 build 20260421 source tree. Authoritative.

## Consensus ordered steps

```python
import bpy
from bl_ext.user_default.mpfb.services.humanservice import HumanService

bpy.ops.wm.read_factory_settings(use_empty=True)

macro = {
    "gender": 0.0,            # 0.0 = FEMALE
    "age": 0.30,              # young adult
    "muscle": 0.60,           # toned, not bodybuilder
    "weight": 0.50,           # average build
    "proportions": 0.55,      # slightly hourglass
    "height": 0.45,           # slightly shorter
    "cupsize": 0.55,          # above-median bust
    "firmness": 0.55,         # firm/athletic
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
basemesh.name = "base_female"

# Use the rig with breast bones (softbody-drivable during animation).
armature = HumanService.add_builtin_rig(basemesh, "game_engine_with_breast", import_weights=True)
armature.name = "base_female.rig"

# ... transform_apply, obj_export, save_as_mainfile identical to male path ...
```

## Gotchas

- **Same gender inversion** as male path.
- `cupsize` and `firmness` only apply when `gender` < 0.5. On male bases these two values are ignored.
- `game_engine_with_breast` rig has 55 bones (53 + breast_l + breast_r). If you don't need the breast bones for soft-body animation, use the plain `game_engine` rig (53 bones).
- Body dimensions only visible on the **evaluated** (shape-key-applied) mesh: use `basemesh.evaluated_get(depsgraph).data.vertices` — the rest-pose `basemesh.data.vertices` is gender-invariant (same topology).
- Female hip width at z~1.0 in the evaluated mesh is ~0.30 m (vs ~0.35 for male). Shoulder width is ~0.42 (vs ~0.49 male). Useful reference when placing armor/clothing.

## Expected output stats

- verts = 19158
- polys = 18486 (100 % quads)
- bone count = 55 (game_engine_with_breast)
- raw height: 1.6946 m; post-scale: 1.912 m

## Post-task update log

- 2026-04-21 — Session 9613f92d — Generated alongside male. Same inversion gotcha hit and corrected. Final render confirms female silhouette (breasts, narrower shoulders, slightly shorter stance). Output: `third_party/base_humanoid/female/base_female.{blend,obj}`.
