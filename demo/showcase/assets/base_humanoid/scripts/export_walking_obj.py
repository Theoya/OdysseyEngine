"""
Export a representative walking-pose .obj at frame 1 (contact R) from the
skinned walking.blend. Mesh is baked with all deformation modifiers applied
so the exported OBJ reflects the walking pose.

Output: third_party/base_humanoid/<gender>/base_<gender>_walking.obj

Usage:
  "<blender4.2>" --background --python export_walking_obj.py -- <male|female>
"""

import os
import sys
import bpy

argv = sys.argv
gender = argv[argv.index("--") + 1] if "--" in argv else "male"
assert gender in ("male", "female")

SRC = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\walking.blend"
DST = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\base_{gender}_walking.obj"

bpy.ops.wm.open_mainfile(filepath=SRC)
bpy.context.scene.frame_set(1)  # contact-R pose

mesh = bpy.data.objects[f"base_{gender}"]
bpy.ops.object.select_all(action="DESELECT")
mesh.select_set(True)
bpy.context.view_layer.objects.active = mesh

bpy.ops.wm.obj_export(
    filepath=DST,
    export_selected_objects=True,
    export_uv=True,
    export_normals=True,
    export_materials=False,
    apply_modifiers=True,          # bake armature+mask deformation
    forward_axis="NEGATIVE_Z",
    up_axis="Y",
)
print(f"exported walking pose (frame 1) to {DST}")
