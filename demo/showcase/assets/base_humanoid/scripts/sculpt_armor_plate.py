"""
Proof-of-workflow armor chest plate, sculpted onto the base humanoid via
the canonical hard-surface pipeline:

  1. identify chest region by world-space bounding box
  2. create a sculpting plane above the torso
  3. subdivide + shrinkwrap to the base mesh (skin the plate to the torso)
  4. solidify (gives the plate thickness + inner face)
  5. bevel (catches the hard white rim highlight)
  6. weighted normal (crisp facets read at long range)
  7. save to tutorial_output.blend

Usage:
  "<blender4.2>" --background --python sculpt_armor_plate.py -- <male|female>
"""

import os
import sys

import bpy
import bmesh

argv = sys.argv
if "--" in argv:
    script_args = argv[argv.index("--") + 1 :]
else:
    script_args = []
if not script_args:
    raise SystemExit("usage: sculpt_armor_plate.py -- <male|female>")
gender = script_args[0]
assert gender in ("male", "female")

OUT_DIR = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}"
SRC_BLEND = os.path.join(OUT_DIR, f"base_{gender}.blend")
DST_BLEND = os.path.join(OUT_DIR, "tutorial_output.blend")

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

basemesh_name = f"base_{gender}"
basemesh = bpy.data.objects[basemesh_name]

# --- 1. identify chest region from world-space bounding box ---
bpy.context.view_layer.update()
world = basemesh.matrix_world
coords = [world @ v.co for v in basemesh.data.vertices]
min_z = min(c.z for c in coords)
max_z = max(c.z for c in coords)
min_x = min(c.x for c in coords)
max_x = max(c.x for c in coords)

# Chest plate = sternum-centered disc. Small and clean beats big and messy
# for a proof-of-workflow demo.
chest_z_mid = min_z + 0.68 * (max_z - min_z)
chest_height = 0.28  # 28cm tall
chest_width = 0.26   # 26cm wide — fits between pec muscles / breasts
print(f"[{gender}] chest Z window: [{chest_z_lo:.3f}, {chest_z_hi:.3f}] mid={chest_z_mid:.3f}")

# --- 2. create a plane at that position, in front of the body (+Y direction) ---
# The OBJ-sourced figure faces -Z, so "in front" is -Z direction.
# In Blender default axes: +Z is up, -Y is "in front of" a character.
# MPFB outputs with feet on +Z axis plane, body facing -Y.
bpy.ops.mesh.primitive_plane_add(
    size=chest_width,
    # Place the plate just in front of the chest, then use NEAREST_SURFACEPOINT
    # shrinkwrap to snap it to the torso. Small plate + NEAREST is safe
    # because the plane area is already inside the torso's envelope.
    location=(0.0, -0.10, chest_z_mid),
    rotation=(1.5708, 0, 0),
)
plate = bpy.context.active_object
plate.name = f"{basemesh_name}_chestplate"

# Scale the plate vertically to match the chest Z window.
plate.scale.z = chest_height / chest_width

# Apply the initial transform so modifiers see a correct base shape.
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

# --- 3. subdivide the plane for shrinkwrap conformance ---
# Enter edit mode, subdivide 6 times = 64x64 grid on the plate.
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.subdivide(number_cuts=6)
bpy.ops.object.mode_set(mode="OBJECT")

# --- 4. shrinkwrap modifier: project onto the base mesh ---
sw = plate.modifiers.new("ArmorShrinkwrap", "SHRINKWRAP")
sw.target = basemesh
# Small plate — NEAREST_SURFACEPOINT gives the cleanest chest conformance.
sw.wrap_method = "NEAREST_SURFACEPOINT"
sw.offset = 0.018  # 1.8 cm standoff — plate sits just above the skin

# --- 5. solidify: give the plate thickness ---
sol = plate.modifiers.new("ArmorSolidify", "SOLIDIFY")
sol.thickness = 0.012
sol.offset = 1.0  # all thickness extrudes outward
sol.use_even_offset = True
sol.use_quality_normals = True

# --- 6. bevel: ease the outer rim so it catches rim light ---
bev = plate.modifiers.new("ArmorBevel", "BEVEL")
bev.width = 0.003
bev.segments = 2
bev.limit_method = "ANGLE"
bev.angle_limit = 0.52  # ~30°

# --- 7. weighted normal: crisp-read facets ---
wn = plate.modifiers.new("ArmorWeightedNormal", "WEIGHTED_NORMAL")
wn.weight = 100

# --- 8. shade smooth so bevels read correctly with weighted normals ---
bpy.ops.object.select_all(action="DESELECT")
plate.select_set(True)
bpy.context.view_layer.objects.active = plate
bpy.ops.object.shade_smooth()

# --- 9. create a matte-dark armor material (Fantasy Etherealism palette) ---
mat = bpy.data.materials.new(name="armor_plate_eth")
mat.use_nodes = True
nodes = mat.node_tree.nodes
# Clear default Principled BSDF and rebuild with the Etherealism look.
for n in list(nodes):
    nodes.remove(n)
out = nodes.new("ShaderNodeOutputMaterial")
bsdf = nodes.new("ShaderNodeBsdfPrincipled")
mat.node_tree.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
# Matte black with subtle bluish tint — reads as "dark steel" under Etherealism
# key light but not pure black (pure black eats bloom).
bsdf.inputs["Base Color"].default_value = (0.035, 0.04, 0.05, 1.0)
# Metallic low-ish — we want edge highlight from rim light, not metal specular everywhere.
bsdf.inputs["Metallic"].default_value = 0.35
bsdf.inputs["Roughness"].default_value = 0.55
plate.data.materials.append(mat)

# --- 10. save authoring .blend (with modifier stack intact) ---
bpy.ops.wm.save_as_mainfile(filepath=DST_BLEND)
print(f"--- armor plate sculpted & saved to {DST_BLEND} ---")
print(f"    plate verts={len(plate.data.vertices)}  modifiers={[m.name for m in plate.modifiers]}")
