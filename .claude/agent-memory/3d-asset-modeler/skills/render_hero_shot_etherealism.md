---
skill: render_hero_shot_etherealism
difficulty: intermediate
prerequisites: [a .blend with the subject character / asset, Blender 4.2+]
status: complete — executed end-to-end 2026-04-21 (12 renders)
blender target: 4.2 LTS (uses Eevee-Next in 4.2)
---

## Goal

Render a cinematic Fantasy Etherealism hero shot of a character / asset:

- Hard white rim / edge highlights carving a silhouette
- Deep black shadows (near-pure black background)
- Subtle bloom, Filmic view transform
- Medium-high contrast "ship-vibe" look matching the Berserk-Halo Mk3 hero shots

## Sources

1. [Blender Guru — Cinematic lighting series](https://www.blenderguru.com/) — 2023-2024 — three-point lighting foundation; we substitute a cool sun for key, warm hard area for rim, dim cool area for fill.
2. [Ian Hubert — "Lazy tutorials" on moody lighting](https://www.youtube.com/@IanHubert2) — 2023-2025 — minimal-rig lighting; single hot rim + darker world = dramatic silhouette.
3. [Blender Studio — Character lighting breakdowns](https://studio.blender.org/) — 2024 — Filmic view transform + high-contrast look pass.
4. [Default Cube — Eevee moody renders](https://www.youtube.com/@DefaultCube) — 2023 — bloom tuning (Blender 4.0/4.2): low intensity (0.04) still reads.
5. [Blender Manual 4.2 — Eevee Next settings](https://docs.blender.org/manual/en/latest/render/eevee/render_settings/) — 2026 — TAA samples, viewport transform options.

(Plus the internal reference `demo/showcase/assets/berserk_halo_mk3/` renders — the touchstone for "what Fantasy Etherealism reads like in this project".)

## Consensus ordered steps

```python
import bpy, math

# 1. Strip any existing lights + cameras.
for obj in list(bpy.data.objects):
    if obj.type in ("LIGHT", "CAMERA"):
        bpy.data.objects.remove(obj, do_unlink=True)
scene = bpy.context.scene

# 2. KEY — cool-white hard sun, 7.0 energy, 0.02 rad angle (sharp shadows).
key = bpy.data.lights.new("EthKey", "SUN"); key.energy = 7.0
key.color = (0.95, 0.97, 1.0); key.angle = 0.02
key_obj = bpy.data.objects.new("EthKey", key); scene.collection.objects.link(key_obj)
key_obj.rotation_euler = (math.radians(55), math.radians(20), math.radians(35))

# 3. RIM — warm-white hot area behind/above, carves hard edge highlight.
rim = bpy.data.lights.new("EthRim", "AREA"); rim.energy = 2000.0
rim.color = (1.0, 0.96, 0.88); rim.size = 1.0
rim_obj = bpy.data.objects.new("EthRim", rim); scene.collection.objects.link(rim_obj)
rim_obj.location = (1.8, 2.2, 2.8)
rim_obj.rotation_euler = (math.radians(-120), math.radians(-30), math.radians(-30))

# 4. FILL — faint cool area from below, just lifts shadow.
fill = bpy.data.lights.new("EthFill", "AREA"); fill.energy = 40.0
fill.color = (0.82, 0.87, 1.0); fill.size = 3.0
fill_obj = bpy.data.objects.new("EthFill", fill); scene.collection.objects.link(fill_obj)
fill_obj.location = (-1.5, -2.0, 0.6)
fill_obj.rotation_euler = (math.radians(90), 0, math.radians(-40))

# 5. WORLD — deep blue-black, strength 0.05.
world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
scene.world = world; world.use_nodes = True
bg = world.node_tree.nodes["Background"]
bg.inputs[0].default_value = (0.005, 0.007, 0.012, 1.0)
bg.inputs[1].default_value = 0.05

# 6. CAMERA — FOV 32-35°, aimed at character waist height.
import mathutils
cam_data = bpy.data.cameras.new("Hero"); cam_data.lens_unit = "FOV"
cam_data.angle = math.radians(32)
cam = bpy.data.objects.new("Hero", cam_data); scene.collection.objects.link(cam)
cam.location = (2.2, -2.3, 1.35)
direction = mathutils.Vector((0, 0, 1.0)) - mathutils.Vector(cam.location)
cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
scene.camera = cam

# 7. RENDER settings.
scene.render.engine = "BLENDER_EEVEE_NEXT"  # Blender 4.2 identifier
scene.render.resolution_x = 1920; scene.render.resolution_y = 1080
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.view_settings.view_transform = "Filmic"
scene.view_settings.look = "Medium High Contrast"
if hasattr(scene, "eevee"):
    scene.eevee.taa_render_samples = 64
    if hasattr(scene.eevee, "use_bloom"): scene.eevee.use_bloom = True
    if hasattr(scene.eevee, "bloom_intensity"): scene.eevee.bloom_intensity = 0.04
    if hasattr(scene.eevee, "use_gtao"): scene.eevee.use_gtao = True

# 8. Render.
scene.render.filepath = "/path/to/hero.png"
bpy.ops.render.render(write_still=True)
```

## Gotchas

- **Blender 4.2 renames Eevee to `BLENDER_EEVEE_NEXT`**. In 2.93 it was `BLENDER_EEVEE`. Use a try/except or feature-detect the engine enum if you target both.
- **Bloom API differs**. Blender 4.0+ dropped some bloom attributes; guard with `hasattr(scene.eevee, "use_bloom")` before setting.
- **Filmic view transform is not automatic**. Default view transform in 4.2 is "AgX" — looks different from Filmic. For "ship vibe" stick to Filmic + Medium High Contrast look.
- **World strength 0.0 is dangerous** — it eats all subsurface lighting and makes character interiors unreadable. 0.03-0.05 is the sweet spot for Etherealism "dark air".
- **Rim light energy 2000 sounds crazy** but area lights in Blender use Watts + inverse-square falloff; at 1.5-3 m subject distance, 2000 W AREA = stark white highlight + clean falloff. Halve to 1000 for close-up shots.
- **Key sun at 7.0 energy** is the upper end of hero-shot brightness for Filmic; above 10 the highlights clip. Below 4, shadows read as smeared.
- **Rim + key angle dependence**: if the rim + key both hit the same side of the subject, you lose the dramatic edge. Keep rim at ~135° from the key.
- **MSAA vs TAA**: Eevee Next uses TAA by default. 64 samples is visibly clean; 32 shows faint grain on smooth skin. Don't bother with MSAA — TAA is better.

## Post-task update log

- 2026-04-21 — Session 9613f92d — Rendered 12 hero shots (2 genders × 2 variants × 3 views = 12). Each 1920×1080 Eevee Next, 64 TAA, Filmic MHC. Load times: 0.3-0.4 sec per frame. Output: `demo/showcase/assets/base_humanoid/renders/*.png`. Palette matches Berserk-Halo visual brief.
