"""
Render 4-angle Fantasy Etherealism hero shots of the male base humanoid
with the torso armor sculpted on top.

Angles:
  - front            : camera at (0, -3.2, 1.25) looking at (0,0,1.3)
  - three_quarter    : camera at (-2.2, -2.3, 1.35) looking at (0,0,1.3)  [3/4 LEFT]
  - side             : camera at (-3.2, 0, 1.25) looking at (0,0,1.3)     [side LEFT]
  - back             : camera at (0, 3.2, 1.25) looking at (0,0,1.3)

Palette:
  Matte-near-black albedo (0.10), matches Berserk-Halo.
  Cool sun key + warm hot area rim + dim cool area fill.
  World strength 0.05, deep blue-black.

Usage:
  "<blender4.2>" --background --python render_male_torso_armor.py
"""

import math
import os
import sys

import bpy
import mathutils

SRC_BLEND = r"T:\OdysseyEngine\third_party\base_humanoid\male\torso_armor.blend"
OUT_DIR   = r"T:\OdysseyEngine\demo\showcase\assets\base_humanoid\renders"
os.makedirs(OUT_DIR, exist_ok=True)

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

# Strip any existing lights/cameras.
for obj in list(bpy.data.objects):
    if obj.type in ("LIGHT", "CAMERA"):
        bpy.data.objects.remove(obj, do_unlink=True)

scene = bpy.context.scene

# --- Defensive: (re)apply dark matte undersuit material to base body, per
# iter-2 defect #3 fix. The sculpt script already sets this, but render
# re-asserts so a stale .blend can't sneak a bright body back into the render.
# Spec: albedo (0.08, 0.08, 0.10), roughness 0.85, metallic 0.0.
basemesh = bpy.data.objects.get("base_male")
if basemesh is not None:
    um = bpy.data.materials.get("body_undersuit_eth") or \
         bpy.data.materials.new(name="body_undersuit_eth")
    um.use_nodes = True
    _nodes = um.node_tree.nodes
    for _n in list(_nodes):
        _nodes.remove(_n)
    _out = _nodes.new("ShaderNodeOutputMaterial")
    _bsdf = _nodes.new("ShaderNodeBsdfPrincipled")
    um.node_tree.links.new(_bsdf.outputs["BSDF"], _out.inputs["Surface"])
    # Iter-2 attempt 4: values tuned to actually render dark after Filmic +
    # hot rim. See sculpt script for rationale.
    _bsdf.inputs["Base Color"].default_value = (0.030, 0.030, 0.045, 1.0)
    _bsdf.inputs["Metallic"].default_value = 0.0
    _bsdf.inputs["Roughness"].default_value = 0.90
    # Suppress specular highlight on body so hot rim doesn't blow it out.
    for _key in ("Specular IOR Level", "Specular"):
        if _key in _bsdf.inputs:
            _bsdf.inputs[_key].default_value = 0.10
            break
    basemesh.data.materials.clear()
    basemesh.data.materials.append(um)
    print(f"[render] applied undersuit material to {basemesh.name}")
else:
    print("[render] WARNING: base_male object not found; undersuit material not applied")

# --- 1. KEY: cool-white hard sun, upper-right ---
key_data = bpy.data.lights.new("EthKey", "SUN")
key_data.energy = 7.0
key_data.color = (0.95, 0.97, 1.0)
key_data.angle = 0.02
key = bpy.data.objects.new("EthKey", key_data)
scene.collection.objects.link(key)
# Rotation from render_hero_shot_etherealism skill guide.
key.rotation_euler = (math.radians(55), math.radians(20), math.radians(35))

# --- 2. RIM: warm-white hot area, back-left (high) ---
rim_data = bpy.data.lights.new("EthRim", "AREA")
rim_data.energy = 2000.0
rim_data.color = (1.0, 0.96, 0.88)
rim_data.size = 1.0
rim = bpy.data.objects.new("EthRim", rim_data)
scene.collection.objects.link(rim)
# Back-left high: +X back, +Y behind character, +Z high
rim.location = (-1.8, 2.2, 2.8)
rim.rotation_euler = (math.radians(-120), math.radians(30), math.radians(30))

# --- 2b. Secondary rim from UPPER-RIGHT (opposite side of key), so chevron
# edges catch specular from both flanks. Lower intensity than main rim.
rim2_data = bpy.data.lights.new("EthRim2", "AREA")
rim2_data.energy = 1200.0
rim2_data.color = (1.0, 0.95, 0.85)
rim2_data.size = 0.8
rim2 = bpy.data.objects.new("EthRim2", rim2_data)
scene.collection.objects.link(rim2)
rim2.location = (1.6, 1.8, 2.4)
rim2.rotation_euler = (math.radians(-115), math.radians(-25), math.radians(-25))

# --- 3. FILL: cool dim area, front-left low ---
fill_data = bpy.data.lights.new("EthFill", "AREA")
fill_data.energy = 40.0
fill_data.color = (0.82, 0.87, 1.0)
fill_data.size = 3.0
fill = bpy.data.objects.new("EthFill", fill_data)
scene.collection.objects.link(fill)
fill.location = (-1.5, -2.0, 0.6)
fill.rotation_euler = (math.radians(90), 0, math.radians(-40))

# --- 4. World: deep blue-black ---
world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
if bg:
    bg.inputs[0].default_value = (0.005, 0.007, 0.012, 1.0)
    bg.inputs[1].default_value = 0.05

# --- 5. Render settings ---
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

# --- 6. Four camera angles ---
# Subject is 1.912m tall, min_z=0. Center-of-mass ~0.95m, chest ~1.35m.
# Aim target at upper body (z=1.15) for a torso-hero framing that still
# shows hips+legs so the armor reads proportionally.
TARGET = mathutils.Vector((0, 0, 1.15))

VIEWS = {
    # Pull cameras further back (4.0m) for full-body framing, lens at chest level.
    "front":         {"location": (0.0,  -4.0, 1.25), "fov_deg": 32},
    "three_quarter": {"location": (-2.9, -2.9, 1.35), "fov_deg": 32},  # 3/4 LEFT
    "side":          {"location": (-4.0,  0.0, 1.25), "fov_deg": 32},  # side LEFT
    "back":          {"location": (0.0,   4.0, 1.25), "fov_deg": 32},
}

def render_view(name, cfg):
    cam_data = bpy.data.cameras.new(f"Cam_{name}")
    cam_data.lens_unit = "FOV"
    cam_data.angle = math.radians(cfg["fov_deg"])
    cam = bpy.data.objects.new(f"Cam_{name}", cam_data)
    scene.collection.objects.link(cam)
    cam.location = cfg["location"]
    direction = TARGET - mathutils.Vector(cam.location)
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam

    fname = f"male_torso_armor_{name}.png"
    scene.render.filepath = os.path.join(OUT_DIR, fname)
    bpy.ops.render.render(write_still=True)
    print(f"  rendered {fname}")

for name, cfg in VIEWS.items():
    render_view(name, cfg)

print("--- male torso-armor renders complete ---")
