"""
Render 4 walking-keyframe hero shots per gender in Fantasy Etherealism style.

Frames captured: 1, 8, 16, 24 (the task-spec quartet). The shipping walk
cycle is 0.8s = 20 frames at 24fps with keys at frames 1, 6, 11, 15, 20.
With the Cycles F-modifier on, frame 24 wraps to frame 4 (=mid-way through
down-pose), giving 4 visibly-different poses along the stride progression.

Camera: three-quarter hero angle (matches Step 1 three_quarter view).

Output: demo/showcase/assets/base_humanoid/renders/<gender>_walking_frame<NN>.png

Usage:
  "<blender4.2>" --background --python render_walking_keyframes.py -- <male|female>
"""

import math
import os
import sys

import bpy
import mathutils

argv = sys.argv
if "--" in argv:
    script_args = argv[argv.index("--") + 1 :]
else:
    script_args = []
if not script_args:
    raise SystemExit("usage: render_walking_keyframes.py -- <male|female>")
gender = script_args[0]
assert gender in ("male", "female")

SRC_BLEND = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\walking.blend"
OUT_DIR = r"T:\OdysseyEngine\demo\showcase\assets\base_humanoid\renders"
os.makedirs(OUT_DIR, exist_ok=True)

# Task-spec frames 1, 8, 16, 24 describe stride progression (contact, down, passing, up).
# The shipping walk cycle is 0.8s (20 frames), with authored keys at 1,6,11,15,20.
# So 1 = contact R (widest stride), 8 = ~down R (weight shift), 16 = ~up R (push-off),
# 24 = wrap to ~frame 4 (early after contact). Four visibly-distinct poses per gender.
FRAMES = [1, 8, 16, 24]

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)
scene = bpy.context.scene

# --- clean lights & cameras, rebuild per Etherealism spec ---
for obj in list(bpy.data.objects):
    if obj.type in ("LIGHT", "CAMERA"):
        bpy.data.objects.remove(obj, do_unlink=True)

# Fantasy Etherealism: cool key + warm rim + cool fill.
key_data = bpy.data.lights.new("EthKey", "SUN")
key_data.energy = 7.0
key_data.color = (0.95, 0.97, 1.0)
key_data.angle = 0.02
key = bpy.data.objects.new("EthKey", key_data)
scene.collection.objects.link(key)
key.rotation_euler = (math.radians(55), math.radians(20), math.radians(35))

rim_data = bpy.data.lights.new("EthRim", "AREA")
rim_data.energy = 2000.0
rim_data.color = (1.0, 0.96, 0.88)
rim_data.size = 1.0
rim = bpy.data.objects.new("EthRim", rim_data)
scene.collection.objects.link(rim)
rim.location = (1.8, 2.2, 2.8)
rim.rotation_euler = (math.radians(-120), math.radians(-30), math.radians(-30))

fill_data = bpy.data.lights.new("EthFill", "AREA")
fill_data.energy = 40.0
fill_data.color = (0.82, 0.87, 1.0)
fill_data.size = 3.0
fill = bpy.data.objects.new("EthFill", fill_data)
scene.collection.objects.link(fill)
fill.location = (-1.5, -2.0, 0.6)
fill.rotation_euler = (math.radians(90), 0, math.radians(-40))

# Deep-black world.
world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
if bg:
    bg.inputs[0].default_value = (0.005, 0.007, 0.012, 1.0)
    bg.inputs[1].default_value = 0.05

# --- three-quarter hero cam ---
# Character stands with +Z up, facing roughly -Y. Place camera off to +X
# and in front (-Y) so we see a 3/4 body angle. Pull back far enough to
# fit head (~1.9m) + walking-pose leg extension without clipping.
cam_data = bpy.data.cameras.new("Cam_walk")
cam_data.lens_unit = "FOV"
cam_data.angle = math.radians(36)
cam = bpy.data.objects.new("Cam_walk", cam_data)
scene.collection.objects.link(cam)
cam.location = (2.8, -4.0, 1.25)
target = mathutils.Vector((0, 0, 1.0))  # aim at mid-torso
direction = target - mathutils.Vector(cam.location)
rot_quat = direction.to_track_quat("-Z", "Y")
cam.rotation_euler = rot_quat.to_euler()
scene.camera = cam

# --- render engine + quality ---
try:
    scene.render.engine = "BLENDER_EEVEE_NEXT"
except TypeError:
    scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 1920
scene.render.resolution_y = 1080
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.view_settings.view_transform = "Filmic"
scene.view_settings.look = "Medium High Contrast"
if hasattr(scene, "eevee"):
    scene.eevee.taa_render_samples = 64
    if hasattr(scene.eevee, "use_bloom"):
        scene.eevee.use_bloom = True
    if hasattr(scene.eevee, "bloom_intensity"):
        scene.eevee.bloom_intensity = 0.04
    if hasattr(scene.eevee, "use_gtao"):
        scene.eevee.use_gtao = True

# --- render at each target frame ---
for frame in FRAMES:
    scene.frame_set(frame)
    fname = f"{gender}_walking_frame{frame:02d}.png"
    scene.render.filepath = os.path.join(OUT_DIR, fname)
    bpy.ops.render.render(write_still=True)
    print(f"  rendered frame {frame} -> {fname}")

print(f"--- walking keyframe renders complete for {gender} ---")
