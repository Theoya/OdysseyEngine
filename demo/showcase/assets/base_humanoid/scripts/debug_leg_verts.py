"""Debug: at frame 1, compare BEFORE-armature-deform and AFTER-deform
positions of a few upper_leg_r vertices, to prove/disprove that the leg
mesh is actually being deformed.
"""
import os, sys, bpy
from mathutils import Vector
argv = sys.argv
gender = argv[argv.index("--") + 1] if "--" in argv else "male"
SRC = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\walking.blend"
bpy.ops.wm.open_mainfile(filepath=SRC)

mesh = bpy.data.objects[f"base_{gender}"]
arm = bpy.data.objects[f"base_{gender}.engine_rig"]

# Check modifier stack.
print(f"Modifier stack: {[m.name+':'+m.type for m in mesh.modifiers]}")
for m in mesh.modifiers:
    if m.type == "MASK":
        print(f"  MASK vgroup: {m.vertex_group}  invert_vertex_group: {m.invert_vertex_group}")
    if m.type == "ARMATURE":
        print(f"  ARMATURE object: {m.object.name if m.object else None}  use_vertex_groups: {m.use_vertex_groups}")

# Pick 10 vertices that are in the upper_leg_r group.
leg_group = mesh.vertex_groups.get("upper_leg_r")
leg_verts_indices = []
for v in mesh.data.vertices:
    for vg in v.groups:
        if vg.group == leg_group.index and vg.weight > 0.5:
            leg_verts_indices.append(v.index)
            break
    if len(leg_verts_indices) >= 5: break
print(f"Found {len(leg_verts_indices)} leg-rigidly-weighted verts: {leg_verts_indices}")

# Evaluate at frame 6 (rest, approximately) and frame 1 (contact R).
depsgraph = bpy.context.evaluated_depsgraph_get()
for frame in [6, 1, 11]:
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_mesh_obj = mesh.evaluated_get(depsgraph)
    eval_mesh = eval_mesh_obj.data
    print(f"--- frame {frame} ---")
    for i in leg_verts_indices:
        if i >= len(eval_mesh.vertices):
            print(f"  vert {i} out of range ({len(eval_mesh.vertices)} verts in evaluated)")
            continue
        rest_co = mesh.data.vertices[i].co
        pose_co = eval_mesh.vertices[i].co
        delta = pose_co - rest_co
        print(f"  v{i:5d} rest=({rest_co.x:+.3f},{rest_co.y:+.3f},{rest_co.z:+.3f}) pose=({pose_co.x:+.3f},{pose_co.y:+.3f},{pose_co.z:+.3f}) delta=({delta.x:+.3f},{delta.y:+.3f},{delta.z:+.3f})")
