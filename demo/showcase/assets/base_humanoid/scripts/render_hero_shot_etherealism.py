"""
Render Fantasy Etherealism hero shots of the base humanoid in T-pose.

Style cues (matches Berserk-Halo Mk3 hero):
  - hard white rim / edge highlights
  - deep black shadows
  - cinematic gloom, subtle bloom
  - Filmic view transform
  - Eevee with 64 TAA samples

Output: <out_dir>/<gender>_<variant>_<view>.png at 1920x1080.

Views rendered per gender x variant:
  - tpose   (straight-on, slight below eyeline)
  - side    (pure side profile)
  - three_quarter (45° hero angle)
Variants: "bare" (no armor), "tutorial" (tutorial_output.blend with armor plate)
Total: 2 genders x 2 variants x 3 views = 12 renders.

Usage:
  "<blender4.2>" --background --python render_hero_shot_etherealism.py -- <gender> <variant>
"""

import math
import os
import sys

import bpy

argv = sys.argv
if "--" in argv:
    script_args = argv[argv.index("--") + 1 :]
else:
    script_args = []
if len(script_args) < 2:
    raise SystemExit("usage: render_hero_shot_etherealism.py -- <male|female> <bare|tutorial>")
gender, variant = script_args[0], script_args[1]
assert gender in ("male", "female")
assert variant in ("bare", "tutorial")

if variant == "bare":
    SRC_BLEND = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\base_{gender}.blend"
    out_tag = "tpose"
else:
    SRC_BLEND = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\tutorial_output.blend"
    out_tag = "tutorial"

OUT_DIR = r"T:\OdysseyEngine\demo\showcase\assets\base_humanoid\renders"
os.makedirs(OUT_DIR, exist_ok=True)

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

# --- camera + lighting: rebuild from scratch for reproducibility ---
for obj in list(bpy.data.objects):
    if obj.type in ("LIGHT", "CAMERA"):
        bpy.data.objects.remove(obj, do_unlink=True)

scene = bpy.context.scene

# Fantasy Etherealism key light: cool-white hard directional, high intensity.
key_data = bpy.data.lights.new("EthKey", "SUN")
key_data.energy = 7.0
key_data.color = (0.95, 0.97, 1.0)   # subtle cool tint
key_data.angle = 0.02                # sharp shadows
key = bpy.data.objects.new("EthKey", key_data)
scene.collection.objects.link(key)
key.rotation_euler = (math.radians(55), math.radians(20), math.radians(35))

# Rim: warm-white small area on the +Z side behind character, very hot to
# carve a hard edge highlight.
rim_data = bpy.data.lights.new("EthRim", "AREA")
rim_data.energy = 2000.0
rim_data.color = (1.0, 0.96, 0.88)
rim_data.size = 1.0
rim = bpy.data.objects.new("EthRim", rim_data)
scene.collection.objects.link(rim)
rim.location = (1.8, 2.2, 2.8)
rim.rotation_euler = (math.radians(-120), math.radians(-30), math.radians(-30))

# Fill: faint cool bounce from below, barely lifting shadow.
fill_data = bpy.data.lights.new("EthFill", "AREA")
fill_data.energy = 40.0
fill_data.color = (0.82, 0.87, 1.0)
fill_data.size = 3.0
fill = bpy.data.objects.new("EthFill", fill_data)
scene.collection.objects.link(fill)
fill.location = (-1.5, -2.0, 0.6)
fill.rotation_euler = (math.radians(90), 0, math.radians(-40))

# --- World: deep black, no HDRI ambient (Etherealism wants dark air) ---
world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
if bg:
    bg.inputs[0].default_value = (0.005, 0.007, 0.012, 1.0)
    bg.inputs[1].default_value = 0.05

# --- Camera placement depends on the view ---
VIEW_POSES = {
    "tpose": {
        "location": (0, -3.2, 1.25),
        "target": (0, 0, 1.0),
        "fov_deg": 35,
    },
    "side": {
        "location": (3.2, 0, 1.25),
        "target": (0, 0, 1.0),
        "fov_deg": 35,
    },
    "three_quarter": {
        "location": (2.2, -2.3, 1.35),
        "target": (0, 0, 1.0),
        "fov_deg": 32,
    },
}

def add_camera_and_render(view_name, pose):
    cam_data = bpy.data.cameras.new(f"Cam_{view_name}")
    cam_data.lens_unit = "FOV"
    cam_data.angle = math.radians(pose["fov_deg"])
    cam = bpy.data.objects.new(f"Cam_{view_name}", cam_data)
    scene.collection.objects.link(cam)
    cam.location = pose["location"]
    # Aim at target.
    direction = (
        pose["target"][0] - pose["location"][0],
        pose["target"][1] - pose["location"][1],
        pose["target"][2] - pose["location"][2],
    )
    import mathutils
    rot_quat = mathutils.Vector(direction).to_track_quat("-Z", "Y")
    cam.rotation_euler = rot_quat.to_euler()
    scene.camera = cam

    scene.render.engine = "BLENDER_EEVEE_NEXT" if hasattr(scene, "eevee") and "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    # Blender 4.2 uses "BLENDER_EEVEE_NEXT" as the default Eevee identifier.
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    # View transform = Filmic for the cinematic shadow rolloff.
    scene.view_settings.view_transform = "Filmic"
    scene.view_settings.look = "Medium High Contrast"
    # Eevee settings.
    if hasattr(scene, "eevee"):
        scene.eevee.taa_render_samples = 64
        if hasattr(scene.eevee, "use_bloom"):
            scene.eevee.use_bloom = True
        if hasattr(scene.eevee, "bloom_intensity"):
            scene.eevee.bloom_intensity = 0.04
        if hasattr(scene.eevee, "use_gtao"):
            scene.eevee.use_gtao = True

    fname = f"{gender}_{out_tag}_{view_name}.png"
    scene.render.filepath = os.path.join(OUT_DIR, fname)
    bpy.ops.render.render(write_still=True)
    print(f"  rendered {fname}")

for view_name, pose in VIEW_POSES.items():
    add_camera_and_render(view_name, pose)

print("--- Etherealism hero-shot render complete ---")
