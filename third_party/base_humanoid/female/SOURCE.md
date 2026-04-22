# base_female — SOURCE

## Generator

- **Tool:** MPFB2 (MakeHuman Plugin For Blender 2) v2.0.15 build `20260421`
- **Host:** Blender 4.2.20 LTS (portable), built 2026-04-21
- **Host path:** `C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\blender.exe`

## Authoring inputs

- **Macro detail dict** (passed to `HumanService.create_human(macro_detail_dict=...)`):

  ```python
  {
      "gender": 1.0,          # fully female
      "age": 0.30,            # young adult
      "muscle": 0.60,         # toned, not bodybuilder
      "weight": 0.50,         # average build
      "proportions": 0.55,    # slightly hourglass
      "height": 0.45,         # slightly shorter than average
      "cupsize": 0.55,        # slightly above median
      "firmness": 0.55,       # athletic
      "race": {"asian": 0.20, "caucasian": 0.60, "african": 0.20},
  }
  ```

- **Rig:** MPFB built-in `"game_engine_with_breast"` (55 bones — adds two
  breast bones for soft-body animation later).
- **Scale / position:** post-generation uniform scale to reach total world
  height 1.912 m, then translated so feet plant at Z=0.

## Generation script

`T:\OdysseyEngine\demo\showcase\assets\base_humanoid\scripts\generate_base_female.py`
followed by
`T:\OdysseyEngine\demo\showcase\assets\base_humanoid\scripts\scale_to_engine_skeleton.py -- female`

## Outputs

- `base_female.blend`
- `base_female.obj`
- `stats_female.json`

## Geometry stats (post-scale)

- verts = 19158
- polys = 18486 (100 % quads, 0 tris, 0 n-gons)
- OBJ face entries = 13378 (triangulates to ~26756 tris on engine load)
- height = 1.912 m
- bone count = 55
- feet planted at Z = 0.0

## License

CC0 / Public Domain. See `LICENSE.txt`.

## Use in OdysseyEngine

- Descriptor: `demo/fps_humanoid/assets/base_female.mesh.xml`
- Same renderer caveat as `base_male` — parseable, not yet drawn; staged
  for a future skinned-mesh renderer.
