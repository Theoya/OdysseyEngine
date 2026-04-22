"""
Generate a shapely, attractive CC0 female humanoid base via MPFB2.

Blender target: 4.2.20 LTS portable at
  C:\\Users\\THadfield\\Blender 4.2\\blender-4.2.20-windows-x64\\blender.exe
"""

import os
import json

import bpy

from bl_ext.user_default.mpfb.services.humanservice import HumanService

OUT_DIR = r"T:\OdysseyEngine\third_party\base_humanoid\female"
BLEND_PATH = os.path.join(OUT_DIR, "base_female.blend")
OBJ_PATH = os.path.join(OUT_DIR, "base_female.obj")

os.makedirs(OUT_DIR, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)

# Macro detail dict for a shapely, attractive female.
#   gender=1.0     -> fully female
#   age=0.30       -> young adult
#   muscle=0.60    -> toned but not bodybuilder
#   weight=0.50    -> average build, not thin, not heavy
#   proportions=0.55 -> slightly hourglass (amplifies waist-hip ratio)
#   height=0.45    -> slightly shorter than average (heroic-not-amazonian)
#   cupsize=0.55   -> slightly above median
#   firmness=0.55  -> firm, athletic
macro_detail_dict = {
    # MPFB gender slider: 0.0 = female, 1.0 = male (confirmed 2026-04-21 by
    # inspecting data/targets/macrodetails/macro.json — parts[0] has
    # low='female', high='male').
    "gender": 0.0,
    "age": 0.30,
    "muscle": 0.60,
    "weight": 0.50,
    "proportions": 0.55,
    "height": 0.45,
    "cupsize": 0.55,
    "firmness": 0.55,
    "race": {
        "asian": 0.20,
        "caucasian": 0.60,
        "african": 0.20,
    },
}

basemesh = HumanService.create_human(
    mask_helpers=True,
    detailed_helpers=False,
    extra_vertex_groups=True,
    feet_on_ground=True,
    scale=0.1,
    macro_detail_dict=macro_detail_dict,
)
basemesh.name = "base_female"
print(f"--- basemesh: verts={len(basemesh.data.vertices)} polys={len(basemesh.data.polygons)}")

# For a female base with breast geometry, use the game_engine_with_breast rig
# — adds two breast bones that downstream animations / IK can drive.
armature = HumanService.add_builtin_rig(basemesh, "game_engine_with_breast", import_weights=True)
armature.name = "base_female.rig"
print(f"--- rig: bones={len(armature.data.bones)}")

bpy.context.view_layer.objects.active = basemesh
basemesh.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

world_matrix = basemesh.matrix_world
coords = [world_matrix @ v.co for v in basemesh.data.vertices]
height = max(c.z for c in coords) - min(c.z for c in coords)

n_quads = sum(1 for p in basemesh.data.polygons if len(p.vertices) == 4)
n_tris = sum(1 for p in basemesh.data.polygons if len(p.vertices) == 3)
n_ngons = sum(1 for p in basemesh.data.polygons if len(p.vertices) > 4)
total_polys = len(basemesh.data.polygons)
quad_pct = 100.0 * n_quads / max(1, total_polys)
print(f"  raw height: {height:.4f}  quads={n_quads} tris={n_tris} ngons={n_ngons}")

bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
print(f"--- .blend saved to {BLEND_PATH} ---")

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

stats = {
    "gender": "female",
    "verts": len(basemesh.data.vertices),
    "polys": total_polys,
    "quads": n_quads,
    "tris": n_tris,
    "ngons": n_ngons,
    "quad_pct": round(quad_pct, 1),
    "raw_height_blender_units": round(height, 4),
    "bone_count": len(armature.data.bones),
    "macro_detail_dict": macro_detail_dict,
    "rig": "game_engine_with_breast",
}
with open(os.path.join(OUT_DIR, "stats_female.json"), "w") as f:
    json.dump(stats, f, indent=2)
print(json.dumps(stats, indent=2))
