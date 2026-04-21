"""Render a 3/4 hero shot of the Berserk-Halo Mk3 Mjolnir with Eevee.

Run:
    "C:/Program Files/Blender Foundation/Blender 2.93/blender.exe" \
        --background \
        --python render_hero_shot.py

Outputs:
    berserk_halo_hero.png   — 1920x1080 hero 3/4 shot (Eevee render)

Lighting: hard key light from above-right, dim blue fill, black background.
Palette locked to the project "Fantasy Etherealism Impressionism" pillar:
matte-black armor, near-white edge highlights, a single glowing visor.
"""
import bpy
import os
from math import radians

# Load the authoring .blend (all 63 named parts already parented to
# berserk_halo_root, with solidify/bevel/weighted-normal modifiers still live).
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd()
BLEND = os.path.join(SCRIPT_DIR, "berserk_halo_master.blend")
OUT   = os.path.join(SCRIPT_DIR, "berserk_halo_hero.png")

bpy.ops.wm.open_mainfile(filepath=BLEND)

# The build script authors the figure along +Y (for engine export with
# axis_up="Y"). Blender's viewport/world convention is +Z up. Rotate the
# root empty 90deg about X so the figure stands up in the render view.
root_empty = bpy.data.objects.get("berserk_halo_root")
if root_empty:
    root_empty.rotation_euler = (radians(90), 0, 0)

# ---------------------------------------------------------------------------
# Materials — single matte armor material + one emissive visor.
# ---------------------------------------------------------------------------
armor_mat = bpy.data.materials.new("HeroArmor")
armor_mat.use_nodes = True
bsdf = armor_mat.node_tree.nodes.get("Principled BSDF")
if bsdf:
    bsdf.inputs["Base Color"].default_value = (0.05, 0.05, 0.06, 1.0)
    bsdf.inputs["Metallic"].default_value = 0.15
    bsdf.inputs["Roughness"].default_value = 0.70
    bsdf.inputs["Specular"].default_value = 0.3

visor_mat = bpy.data.materials.new("HeroVisor")
visor_mat.use_nodes = True
v_bsdf = visor_mat.node_tree.nodes.get("Principled BSDF")
if v_bsdf:
    v_bsdf.inputs["Emission"].default_value = (1.0, 0.95, 0.55, 1.0)
    v_bsdf.inputs["Emission Strength"].default_value = 6.0 if "Emission Strength" in v_bsdf.inputs else 1.0

# Assign materials to all meshes.
for obj in bpy.data.objects:
    if obj.type != "MESH":
        continue
    obj.data.materials.clear()
    if obj.name == "visor_slit":
        obj.data.materials.append(visor_mat)
    else:
        obj.data.materials.append(armor_mat)

# ---------------------------------------------------------------------------
# Camera: 3/4 hero angle, slightly below eye-line for menace.
# ---------------------------------------------------------------------------
# Clean out any old cameras/lights.
for obj in list(bpy.data.objects):
    if obj.type in ("CAMERA", "LIGHT"):
        bpy.data.objects.remove(obj, do_unlink=True)

# Figure is now upright along +Z (after the root rotation above).
# 3/4 hero angle: offset in +X and +Y, looking back toward origin at chest height.
# Chest is at world z=1.25 (was y=1.25 before the X rotation).
bpy.ops.object.camera_add(location=(2.8, -3.5, 1.25))
cam = bpy.context.active_object
cam.name = "HeroCam"
cam.data.lens = 40
bpy.context.scene.camera = cam

# Aim at chest.
bpy.ops.object.empty_add(type="PLAIN_AXES", location=(0, 0, 1.25))
aim = bpy.context.active_object
aim.name = "HeroAim"
con = cam.constraints.new("TRACK_TO")
con.target = aim
con.track_axis = "TRACK_NEGATIVE_Z"
con.up_axis = "UP_Y"

# ---------------------------------------------------------------------------
# Lighting: hard key, soft rim, cold fill — Fantasy Etherealism pillar.
# ---------------------------------------------------------------------------
# Key light — warm sun from upper-right.
bpy.ops.object.light_add(type="SUN", location=(4, -3, 5))
key = bpy.context.active_object
key.name = "KeyLight"
key.data.energy = 4.5
key.data.color = (1.0, 0.92, 0.75)
key.rotation_euler = (radians(50), radians(10), radians(-35))

# Rim light — hard white from behind-left, carves silhouette.
bpy.ops.object.light_add(type="SPOT", location=(-2.5, 3.2, 1.5))
rim = bpy.context.active_object
rim.name = "RimLight"
rim.data.energy = 2500.0
rim.data.spot_size = radians(60)
rim.data.color = (1.0, 1.0, 1.0)
rim.rotation_euler = (radians(95), 0, radians(215))

# Cold fill — very dim blue from left to hint at fantasy-etherealism.
bpy.ops.object.light_add(type="AREA", location=(-3.5, -2.0, 1.0))
fill = bpy.context.active_object
fill.name = "FillLight"
fill.data.energy = 120.0
fill.data.size = 3.0
fill.data.color = (0.55, 0.65, 0.90)
fill.rotation_euler = (radians(75), radians(-15), radians(-90))

# ---------------------------------------------------------------------------
# World: black background.
# ---------------------------------------------------------------------------
world = bpy.context.scene.world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
if bg:
    bg.inputs["Color"].default_value = (0.02, 0.02, 0.025, 1.0)
    bg.inputs["Strength"].default_value = 0.5

# ---------------------------------------------------------------------------
# Render settings — Eevee for speed.
# ---------------------------------------------------------------------------
scn = bpy.context.scene
scn.render.engine = "BLENDER_EEVEE"
scn.eevee.taa_render_samples = 64
scn.eevee.use_bloom = True
scn.eevee.bloom_intensity = 0.3
scn.eevee.use_ssr = True
scn.eevee.use_gtao = True
scn.render.resolution_x = 1920
scn.render.resolution_y = 1080
scn.render.resolution_percentage = 100
scn.render.image_settings.file_format = "PNG"
scn.render.image_settings.color_mode = "RGBA"
scn.render.film_transparent = False
scn.render.filepath = OUT

# Color management — filmic for crispy darks.
scn.view_settings.view_transform = "Filmic"
scn.view_settings.look = "High Contrast"

# Go.
bpy.ops.render.render(write_still=True)
print(f"[hero] rendered: {OUT}")
