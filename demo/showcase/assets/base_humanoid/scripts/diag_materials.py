import bpy
bpy.ops.wm.open_mainfile(filepath=r"T:\OdysseyEngine\third_party\base_humanoid\male\torso_armor.blend")
print("=== materials per object ===")
for obj in bpy.context.scene.objects:
    if obj.type != "MESH":
        continue
    mats = [s.material.name if s.material else "<none>" for s in obj.material_slots]
    print(f"  {obj.name:28s}  mat_slots={mats}")
print("\n=== all materials in .blend ===")
for m in bpy.data.materials:
    print(f"  {m.name}")
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.type == "BSDF_PRINCIPLED":
                bc = n.inputs["Base Color"].default_value
                print(f"    base_color={tuple(round(x,3) for x in bc)}")
