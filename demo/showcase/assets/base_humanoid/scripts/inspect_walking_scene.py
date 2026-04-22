"""Quick diagnostic: list objects in walking.blend to find stray artifacts."""
import os
import sys
import bpy

argv = sys.argv
if "--" in argv:
    gender = argv[argv.index("--") + 1]
else:
    gender = "male"
SRC = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\walking.blend"
bpy.ops.wm.open_mainfile(filepath=SRC)
print(f"=== {gender} walking.blend objects ===")
for obj in bpy.data.objects:
    print(f"  {obj.type:10s} {obj.name:40s}  dims=({obj.dimensions.x:.3f}, {obj.dimensions.y:.3f}, {obj.dimensions.z:.3f})")
print()
print("=== mesh objects vertex counts ===")
for obj in bpy.data.objects:
    if obj.type == "MESH":
        print(f"  {obj.name}: {len(obj.data.vertices)} verts, {len(obj.data.polygons)} polys, {len(obj.vertex_groups)} vgroups, modifiers={[m.type for m in obj.modifiers]}")
print()
print("=== armatures ===")
for obj in bpy.data.objects:
    if obj.type == "ARMATURE":
        print(f"  {obj.name}: {len(obj.data.bones)} bones")
        if obj.animation_data and obj.animation_data.action:
            print(f"    action={obj.animation_data.action.name} fcurves={len(obj.animation_data.action.fcurves)}")
