"""
Phase 4 — Hero renders of the cloaked Obsidian Sentinel.

Opens cloaked_walking.blend, seeks to mid-walk, applies Fantasy-Etherealism
materials (matte near-black body, dark-violet cloak, minimal hard edge
highlights) and renders 4 angles at 1920x1080:

  - front.png          camera directly in front  (at -Y looking +Y)
  - three_quarter.png  camera at +X,-Y           (classic hero 3/4)
  - side.png           camera at +X              (looking along -X)
  - back.png           camera at +Y              (looking along -Y)

Character faces -Y in world space (verified in debug_facing.py).

Run:
  "C:\\Users\\THadfield\\Blender 4.2\\blender-4.2.20-windows-x64\\blender.exe" \
      --background --python phase4_hero_renders.py
"""

import bpy
import os
import math
import mathutils

PROJECT_ROOT = r"T:/OdysseyEngine"
BLEND = os.path.join(PROJECT_ROOT, "third_party", "obsidian_sentinel_cloaked", "cloaked_walking.blend")
OUT_DIR = os.path.join(PROJECT_ROOT, "demo", "showcase", "assets",
                       "obsidian_sentinel_cloaked", "renders")
os.makedirs(OUT_DIR, exist_ok=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)

scn = bpy.context.scene
# Mid-walk frame — frame 12 of the 0..25 walk, mid-stride (feet visually apart)
MID_WALK = 12
scn.frame_set(MID_WALK)

# ----------------------------------------------------------
# Materials — Fantasy Etherealism palette
# ----------------------------------------------------------
cloak = bpy.data.objects.get("cloak")
body = next((o for o in bpy.data.objects if o.type == "MESH" and o.name != "cloak"), None)
assert cloak and body

# Body: matte near-black with hard metallic highlight — reads as obsidian/armor
mat_body = bpy.data.materials.new("obsidian_body_mat")
mat_body.use_nodes = True
nodes = mat_body.node_tree.nodes
bsdf = nodes.get("Principled BSDF")
if bsdf:
    # Matte-dark: albedo 0.04 (middle of the 0.03-0.08 trick) — keeps detail readable
    # without blowing out under sun light. Slightly cool.
    bsdf.inputs["Base Color"].default_value = (0.04, 0.04, 0.05, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.40
    if "Metallic" in bsdf.inputs:
        bsdf.inputs["Metallic"].default_value = 0.7   # more metallic for crisper edge highlights
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.7
body.data.materials.clear(); body.data.materials.append(mat_body)

# Cloak: dark charcoal-violet, matte. No metallic on cloth.
mat_cloak = bpy.data.materials.new("cloak_mat")
mat_cloak.use_nodes = True
bsdf_c = mat_cloak.node_tree.nodes.get("Principled BSDF")
if bsdf_c:
    # Low-value charcoal with a hint of violet — previous pass was too purple & sheen blew out pink.
    bsdf_c.inputs["Base Color"].default_value = (0.035, 0.028, 0.045, 1.0)
    bsdf_c.inputs["Roughness"].default_value = 0.95  # fully diffuse fabric
    if "Metallic" in bsdf_c.inputs:
        bsdf_c.inputs["Metallic"].default_value = 0.0
    if "Sheen Weight" in bsdf_c.inputs:
        bsdf_c.inputs["Sheen Weight"].default_value = 0.0  # DISABLED — was catching rim too aggressively
cloak.data.materials.clear(); cloak.data.materials.append(mat_cloak)

# Smooth-shade both cloak and body (remove faceted reading on cape)
for o in (cloak, body):
    for p in o.data.polygons:
        p.use_smooth = True
# Also add auto-smooth on the cloak via a weighted-normal pass to reduce faceting
# across its 34-segment arc
bpy.ops.object.select_all(action="DESELECT")
cloak.select_set(True); bpy.context.view_layer.objects.active = cloak
# In Blender 4.2, auto_smooth is controlled via a modifier or the mesh.smooth_normals
# Quick tactic: add EdgeSplit modifier disabled + adjust auto-smooth angle
try:
    cloak.data.use_auto_smooth = True
    cloak.data.auto_smooth_angle = math.radians(60)
except AttributeError:
    # Blender 4.2 deprecated use_auto_smooth on mesh; needs Shade Auto Smooth op
    try:
        bpy.ops.object.shade_auto_smooth(angle=math.radians(60))
    except Exception:
        pass

# ----------------------------------------------------------
# World — deep shadowed background with subtle gradient
# ----------------------------------------------------------
if scn.world is None:
    scn.world = bpy.data.worlds.new("World")
scn.world.use_nodes = True
wnt = scn.world.node_tree
# Wipe existing; build simple deep dark-violet gradient.
for n in list(wnt.nodes):
    wnt.nodes.remove(n)
out = wnt.nodes.new("ShaderNodeOutputWorld")
bg = wnt.nodes.new("ShaderNodeBackground")
bg.inputs[0].default_value = (0.012, 0.010, 0.018, 1.0)   # near black with violet tint
bg.inputs[1].default_value = 1.0
wnt.links.new(bg.outputs[0], out.inputs[0])

# ----------------------------------------------------------
# Lighting — 3-point Fantasy Etherealism:
#   KEY   = cool pale above-front (cold-ethereal)
#   RIM   = warm violet/magenta behind-left (rim-light silhouette)
#   FILL  = subtle cool fill from -X (doesn't flatten)
# Lights are placed relative to the character's FACING direction (-Y).
# ----------------------------------------------------------
# Purge any lights from prior renders (phase2/3 visual_check scripts add lights to the blend).
for o in list(bpy.data.objects):
    if o.type == "LIGHT":
        bpy.data.objects.remove(o, do_unlink=True)

def add_area_light(name, loc, rot_euler_deg, energy, size, color):
    bpy.ops.object.light_add(type="AREA", location=loc)
    lo = bpy.context.object
    lo.name = name
    lo.data.energy = energy
    lo.data.size = size
    lo.data.color = color
    lo.rotation_mode = "XYZ"
    lo.rotation_euler = tuple(math.radians(d) for d in rot_euler_deg)
    return lo

# Ethereal lighting rebalanced — previous pass had rim too hot & too magenta, cloak
# read pink. Fix: key light dominates (energy=900), rim is small/cool, fill minimal.
# KEY: cool pale, upper-front (character faces -Y so "front" = -Y)
add_area_light(
    "key_light",
    loc=(1.0, -3.2, 3.2),
    rot_euler_deg=(60, 10, 15),
    energy=900.0, size=2.0,
    color=(0.90, 0.95, 1.0),
)
# RIM: tight, cool-white, behind-up-and-left — paint silhouette edge only
add_area_light(
    "rim_light",
    loc=(0.5, 2.5, 2.8),
    rot_euler_deg=(105, 0, 180),
    energy=250.0, size=0.6,     # small size + lower energy = tight rim, not wrap-around
    color=(0.8, 0.75, 1.0),     # barely-violet cool, NOT hot magenta
)
# FILL: low, from character-right (-X) — very subtle
add_area_light(
    "fill_light",
    loc=(-3.0, -1.8, 1.5),
    rot_euler_deg=(80, 10, -35),
    energy=80.0, size=2.5,
    color=(0.7, 0.85, 1.0),
)
# HEM spill: faint up-lit violet glow at floor level — catches hem sway
add_area_light(
    "hem_spill",
    loc=(0.0, 0.3, 0.08),
    rot_euler_deg=(-85, 0, 0),
    energy=35.0, size=2.5,
    color=(0.85, 0.6, 1.0),
)

# ----------------------------------------------------------
# Ground plane (for shadow catch & slight floor reflection)
# ----------------------------------------------------------
# Remove any prior ground planes
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.name == "ground":
        bpy.data.objects.remove(o, do_unlink=True)

bpy.ops.mesh.primitive_plane_add(size=20, location=(0, 0, 0))
ground = bpy.context.object
ground.name = "ground"
mat_g = bpy.data.materials.new("ground_mat")
mat_g.use_nodes = True
g_bsdf = mat_g.node_tree.nodes.get("Principled BSDF")
if g_bsdf:
    g_bsdf.inputs["Base Color"].default_value = (0.01, 0.01, 0.015, 1.0)
    g_bsdf.inputs["Roughness"].default_value = 0.35
    if "Metallic" in g_bsdf.inputs:
        g_bsdf.inputs["Metallic"].default_value = 0.6
ground.data.materials.clear(); ground.data.materials.append(mat_g)

# ----------------------------------------------------------
# Render settings — 1920x1080, Eevee-Next, Filmic High Contrast
# ----------------------------------------------------------
scn.render.engine = "BLENDER_EEVEE_NEXT"
scn.eevee.taa_render_samples = 64
scn.view_settings.view_transform = "Filmic"
scn.view_settings.look = "High Contrast"
scn.view_settings.exposure = 0.3
scn.view_settings.gamma = 1.0
scn.render.resolution_x = 1920
scn.render.resolution_y = 1080
scn.render.resolution_percentage = 100
scn.render.image_settings.file_format = "PNG"
scn.render.image_settings.color_mode = "RGB"
scn.render.film_transparent = False

# Enable bloom-like effect if supported (Eevee-Next uses compositor glare for bloom).
# Quick approximation: raise exposure slightly and let high-contrast look add bloom-feel.
# True bloom in EEVEE_NEXT requires compositor nodes; skipped for this pass.

# ----------------------------------------------------------
# Helper — aim a camera at a world-point
# ----------------------------------------------------------
def aim_camera_at(cam_obj, target):
    cam_obj.rotation_mode = "QUATERNION"
    direction = (target - cam_obj.location)
    cam_obj.rotation_quaternion = direction.to_track_quat("-Z", "Y")

# Pick a look-at target a bit above center — mid-torso at ~1.2 m
look_target = mathutils.Vector((0.0, 0.0, 1.05))

# ----------------------------------------------------------
# Four shot definitions
# Character faces -Y. "Front" = -Y side. "Back" = +Y side.
# ----------------------------------------------------------
shots = [
    # (name, camera_pos, lens_mm)
    ("front",         mathutils.Vector((0.0, -3.6, 1.15)), 50),
    ("three_quarter", mathutils.Vector((2.6, -2.9, 1.25)), 55),
    ("side",          mathutils.Vector((3.8, -0.2, 1.20)), 55),
    ("back",          mathutils.Vector((0.3, 3.8, 1.25)),  50),
]

for name, pos, lens in shots:
    # Remove prior camera
    for o in list(bpy.data.objects):
        if o.type == "CAMERA":
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.camera_add(location=pos)
    cam = bpy.context.object
    cam.name = f"cam_{name}"
    cam.data.lens = lens
    cam.data.clip_start = 0.05
    cam.data.clip_end = 50.0
    aim_camera_at(cam, look_target)
    scn.camera = cam
    out_path = os.path.join(OUT_DIR, f"{name}.png")
    scn.render.filepath = out_path
    print(f"Rendering {name} → {out_path}")
    bpy.ops.render.render(write_still=True)

print()
print("=" * 70); print("PHASE 4 COMPLETE"); print("=" * 70)
print(f"Renders written to: {OUT_DIR}")
for name, _, _ in shots:
    print(f"  {name}.png")
