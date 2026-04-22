"""
Normalize MPFB2 base mesh to the OdysseyEngine 19-bone humanoid skeleton
conventions: total height ~1.8 m, feet planted at Y=0, root/pelvis near
Y=0.95. Apply transforms. Re-export .blend + .obj.

Usage:
  "<blender4.2>" --background --python scale_to_engine_skeleton.py -- <gender>
where <gender> is "male" or "female".
"""

import os
import sys
import json

import bpy

# argv after "--" is passed to the script
argv = sys.argv
if "--" in argv:
    script_args = argv[argv.index("--") + 1 :]
else:
    script_args = []
if not script_args:
    raise SystemExit("usage: scale_to_engine_skeleton.py -- <male|female>")
gender = script_args[0]
assert gender in ("male", "female"), f"bad gender: {gender}"

OUT_DIR = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}"
SRC_BLEND = os.path.join(OUT_DIR, f"base_{gender}.blend")
DST_BLEND = os.path.join(OUT_DIR, f"base_{gender}.blend")  # overwrite
DST_OBJ = os.path.join(OUT_DIR, f"base_{gender}.obj")

TARGET_HEIGHT = 1.80  # metres. Engine skeleton default pose height.

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

basemesh_name = f"base_{gender}"
basemesh = bpy.data.objects[basemesh_name]
armature = bpy.data.objects[f"{basemesh_name}.rig"]

# Compute current world-space height (pre-scale).
bpy.context.view_layer.update()
world = basemesh.matrix_world
coords = [world @ v.co for v in basemesh.data.vertices]
ys = [c.z for c in coords]
pre_height = max(ys) - min(ys)
pre_min_z = min(ys)
scale_factor = TARGET_HEIGHT / pre_height
print(f"[{gender}] pre-scale height={pre_height:.4f} min_z={pre_min_z:.4f} scale_factor={scale_factor:.4f}")

# Uniform-scale the armature (mesh is parented, so it will follow).
# Select the armature, apply scale on both objects.
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
basemesh.select_set(True)
bpy.context.view_layer.objects.active = armature
armature.scale = (scale_factor, scale_factor, scale_factor)
basemesh.scale = (scale_factor, scale_factor, scale_factor)
# Apply scale to bake it into geometry + bone rest positions.
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

# Translate so feet plant at Y=0 (engine convention).
bpy.context.view_layer.update()
world = basemesh.matrix_world
coords = [world @ v.co for v in basemesh.data.vertices]
ys = [c.z for c in coords]
min_z_after_scale = min(ys)
# Move armature up/down so minimum Z of mesh = 0.
y_shift = -min_z_after_scale
print(f"[{gender}] post-scale min_z={min_z_after_scale:.4f} y_shift={y_shift:.4f}")

armature.location = (armature.location.x, armature.location.y, armature.location.z + y_shift)
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
basemesh.select_set(True)
bpy.context.view_layer.objects.active = armature
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

# Final report.
bpy.context.view_layer.update()
world = basemesh.matrix_world
coords = [world @ v.co for v in basemesh.data.vertices]
ys = [c.z for c in coords]
final_height = max(ys) - min(ys)
final_min_z = min(ys)
final_max_z = max(ys)
print(f"[{gender}] FINAL height={final_height:.4f} min_z={final_min_z:.4f} max_z={final_max_z:.4f}")

# Stats update.
stats = {
    "gender": gender,
    "verts": len(basemesh.data.vertices),
    "polys": len(basemesh.data.polygons),
    "quads": sum(1 for p in basemesh.data.polygons if len(p.vertices) == 4),
    "tris": sum(1 for p in basemesh.data.polygons if len(p.vertices) == 3),
    "ngons": sum(1 for p in basemesh.data.polygons if len(p.vertices) > 4),
    "height_m": round(final_height, 4),
    "feet_planted_at_z": round(final_min_z, 4),
    "top_of_head_z": round(final_max_z, 4),
    "bone_count": len(armature.data.bones),
}
print(json.dumps(stats, indent=2))
with open(os.path.join(OUT_DIR, f"stats_{gender}.json"), "w") as f:
    json.dump(stats, f, indent=2)

# Save .blend (overwrite).
bpy.ops.wm.save_as_mainfile(filepath=DST_BLEND)
print(f"--- .blend re-saved ---")

# Re-export .obj.
bpy.ops.object.select_all(action="DESELECT")
basemesh.select_set(True)
bpy.context.view_layer.objects.active = basemesh
bpy.ops.wm.obj_export(
    filepath=DST_OBJ,
    export_selected_objects=True,
    export_uv=True,
    export_normals=True,
    export_materials=False,
    apply_modifiers=True,
    forward_axis="NEGATIVE_Z",
    up_axis="Y",
)
print(f"--- .obj re-exported ---")
