# base_male — SOURCE

## Generator

- **Tool:** MPFB2 (MakeHuman Plugin For Blender 2) v2.0.15 build `20260421`
- **Host:** Blender 4.2.20 LTS (portable), built 2026-04-21
- **Host path:** `C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\blender.exe`
- **MPFB install path:** `%APPDATA%\Blender Foundation\Blender\4.2\extensions\user_default\mpfb\`
- **Install date:** 2026-04-21

## Authoring inputs

- **Macro detail dict** (passed to `HumanService.create_human(macro_detail_dict=...)`):

  ```python
  {
      "gender": 0.0,          # fully male
      "age": 0.30,            # young adult
      "muscle": 0.90,         # muscular
      "weight": 0.55,         # slightly heavy (mass, not fat)
      "proportions": 0.50,    # average
      "height": 0.55,         # slightly tall (later re-normalized to 1.91 m)
      "cupsize": 0.5,         # ignored for male
      "firmness": 0.5,        # ignored for male
      "race": {"asian": 0.20, "caucasian": 0.60, "african": 0.20},
  }
  ```

- **Rig:** MPFB built-in `"game_engine"` (53 bones, no IK controllers, no
  rigify meta-widgets, matches the engine's planned retarget target).
- **Scale / position:** post-generation uniform scale to reach total world
  height 1.912 m, then translated so feet plant at Z=0.
- **Applied transforms:** all location + scale applied into geometry /
  bone rest positions so the OBJ export is self-contained.

## Generation script

`T:\OdysseyEngine\demo\showcase\assets\base_humanoid\scripts\generate_base_male.py`
followed by
`T:\OdysseyEngine\demo\showcase\assets\base_humanoid\scripts\scale_to_engine_skeleton.py -- male`

## Outputs

- `base_male.blend` — authoring file (armature + mesh).
- `base_male.obj` — Blender 4.2 `wm.obj_export` output. Forward `-Z`, up `+Y`.
- `stats_male.json` — final geometry/bone stats.

## Geometry stats (post-scale)

- verts = 19158
- polys = 18486 (100 % quads, 0 tris, 0 n-gons)
- OBJ face entries = 13378 (coalesced; triangulates to ~26756 tris on engine load)
- height = 1.912 m
- bone count = 53
- feet planted at Z = 0.0

## License

CC0 / Public Domain. See `LICENSE.txt`.

## Use in OdysseyEngine

- Descriptor: `demo/fps_humanoid/assets/base_male.mesh.xml` → points at this OBJ.
- The engine's forward renderer currently binds only primitive mesh_types
  (box / sphere / ground / cylinder). This OBJ is parseable via
  tiny_obj_loader but is not yet drawn at runtime. Skinned-mesh rendering
  is a future council-gated subsystem. Use today: external tool validation
  and staging for that skinned-mesh path.
