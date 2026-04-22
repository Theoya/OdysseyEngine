"""
Generate a muscular, attractive CC0 male humanoid base via MPFB2.

Blender target: 4.2.20 LTS portable at
  C:\\Users\\THadfield\\Blender 4.2\\blender-4.2.20-windows-x64\\blender.exe

Run headless:
  "<blender>" --background --python generate_base_male.py

Output:
  T:\\OdysseyEngine\\third_party\\base_humanoid\\male\\base_male.blend
  T:\\OdysseyEngine\\third_party\\base_humanoid\\male\\base_male.obj
  T:\\OdysseyEngine\\third_party\\base_humanoid\\male\\SOURCE.md
  T:\\OdysseyEngine\\third_party\\base_humanoid\\male\\LICENSE.txt
"""

import os
import sys
import json

import bpy

from bl_ext.user_default.mpfb.services.humanservice import HumanService
from bl_ext.user_default.mpfb.services.targetservice import TargetService

OUT_DIR = r"T:\OdysseyEngine\third_party\base_humanoid\male"
BLEND_PATH = os.path.join(OUT_DIR, "base_male.blend")
OBJ_PATH = os.path.join(OUT_DIR, "base_male.obj")

os.makedirs(OUT_DIR, exist_ok=True)

# ---------------------------------------------------------------------------
# Step 1: wipe the default scene (cube, light, camera are fine to keep, but
# the MPFB base-mesh needs to start from a clean object-less state to avoid
# collisions with named objects).
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)

# ---------------------------------------------------------------------------
# Step 2: macro detail dict for a muscular, attractive male.
#
#   gender=0.0 -> fully male
#   age=0.3    -> young adult
#   muscle=0.9 -> muscular
#   weight=0.55 -> slightly above average (mass, not fat)
#   proportions=0.5 -> average proportions
#   height=0.55 -> slightly taller than average
#   cupsize / firmness: male ignores these (MPFB gates on gender)
#   race: balanced heroic caucasian-leaning but not exclusive
#
# Reference: TargetService.get_default_macro_info_dict()
#   file: services/targetservice.py:812
# ---------------------------------------------------------------------------
macro_detail_dict = {
    # MPFB gender slider: 0.0 = female, 1.0 = male (confirmed 2026-04-21 by
    # inspecting data/targets/macrodetails/macro.json — parts[0] has
    # low='female', high='male'). Do NOT flip this; the natural reading of
    # "0 means male" is backwards per MPFB convention.
    "gender": 1.0,
    "age": 0.30,
    "muscle": 0.90,
    "weight": 0.55,
    "proportions": 0.50,
    "height": 0.55,
    "cupsize": 0.5,
    "firmness": 0.5,
    "race": {
        "asian": 0.20,
        "caucasian": 0.60,
        "african": 0.20,
    },
}

# ---------------------------------------------------------------------------
# Step 3: create the human.
#   mask_helpers=True drops the MakeHuman modelling helpers (we don't need
#                     them in the exported mesh).
#   detailed_helpers=False keeps mask_helpers's full effect (less geometry).
#   extra_vertex_groups=True keeps the MH body-part vertex groups so we can
#                     identify limbs / chest / head for later skinning.
#   feet_on_ground=True positions feet at Y=0.
#   scale=0.1 is the MPFB standard (base_humanoid height ~0.18 m in Blender
#             units; we rescale to 1.8 m later in scale_to_engine_skeleton).
# ---------------------------------------------------------------------------
basemesh = HumanService.create_human(
    mask_helpers=True,
    detailed_helpers=False,
    extra_vertex_groups=True,
    feet_on_ground=True,
    scale=0.1,
    macro_detail_dict=macro_detail_dict,
)
basemesh.name = "base_male"

print("--- basemesh created ---")
print("  name:", basemesh.name)
print("  vertex count:", len(basemesh.data.vertices))
print("  polygon count:", len(basemesh.data.polygons))

# ---------------------------------------------------------------------------
# Step 4: add the "game_engine" standard rig (matches the OdysseyEngine
# humanoid skeleton philosophy — compact bone set, no IK controllers, no
# rigify meta-widgets).
# ---------------------------------------------------------------------------
armature = HumanService.add_builtin_rig(basemesh, "game_engine", import_weights=True)
armature.name = "base_male.rig"
print("--- rig added ---")
print("  armature name:", armature.name)
print("  bone count:", len(armature.data.bones))

# ---------------------------------------------------------------------------
# Step 5: apply transforms and report geometry stats.
# ---------------------------------------------------------------------------
bpy.context.view_layer.objects.active = basemesh
basemesh.select_set(True)
# Apply location only; leave rotation/scale for scale_to_engine_skeleton.
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

# Height = world-space max Z - min Z.
world_matrix = basemesh.matrix_world
coords = [world_matrix @ v.co for v in basemesh.data.vertices]
height = max(c.z for c in coords) - min(c.z for c in coords)
print(f"  raw height (Blender units): {height:.4f}")

# Count quads vs triangles.
n_quads = sum(1 for p in basemesh.data.polygons if len(p.vertices) == 4)
n_tris = sum(1 for p in basemesh.data.polygons if len(p.vertices) == 3)
n_ngons = sum(1 for p in basemesh.data.polygons if len(p.vertices) > 4)
total_polys = len(basemesh.data.polygons)
quad_pct = 100.0 * n_quads / max(1, total_polys)
print(f"  polygons: quads={n_quads} tris={n_tris} ngons={n_ngons} quad%={quad_pct:.1f}")

# ---------------------------------------------------------------------------
# Step 6: save .blend.
# ---------------------------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
print(f"--- .blend saved to {BLEND_PATH} ---")

# ---------------------------------------------------------------------------
# Step 7: export .obj.
#   Blender 4.2 API: bpy.ops.wm.obj_export(...)
#   Do NOT use bpy.ops.export_scene.obj (that's the 2.93 form).
# ---------------------------------------------------------------------------
bpy.ops.object.select_all(action="DESELECT")
basemesh.select_set(True)
bpy.context.view_layer.objects.active = basemesh
bpy.ops.wm.obj_export(
    filepath=OBJ_PATH,
    export_selected_objects=True,
    export_uv=True,
    export_normals=True,
    export_materials=False,
    apply_modifiers=True,
    forward_axis="NEGATIVE_Z",
    up_axis="Y",
)
print(f"--- .obj exported to {OBJ_PATH} ---")

# ---------------------------------------------------------------------------
# Step 8: stats dump for the BUILD_LOG / SOURCE.md.
# ---------------------------------------------------------------------------
stats = {
    "gender": "male",
    "verts": len(basemesh.data.vertices),
    "polys": total_polys,
    "quads": n_quads,
    "tris": n_tris,
    "ngons": n_ngons,
    "quad_pct": round(quad_pct, 1),
    "raw_height_blender_units": round(height, 4),
    "bone_count": len(armature.data.bones),
    "macro_detail_dict": macro_detail_dict,
    "rig": "game_engine",
}
with open(os.path.join(OUT_DIR, "stats_male.json"), "w") as f:
    json.dump(stats, f, indent=2)
print("--- stats dumped ---")
print(json.dumps(stats, indent=2))
