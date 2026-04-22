"""Diagnostic: inspect torso_armor.blend and base_male.blend Z landmarks
to diagnose why the armor ended up at face height."""
import bpy

for path, tag in [
    (r"T:\OdysseyEngine\third_party\base_humanoid\male\base_male.blend", "BASE"),
    (r"T:\OdysseyEngine\third_party\base_humanoid\male\torso_armor.blend", "ARMOR"),
]:
    bpy.ops.wm.open_mainfile(filepath=path)
    print(f"\n=== {tag}: {path} ===")
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        bpy.context.view_layer.update()
        mw = obj.matrix_world
        verts = [mw @ v.co for v in obj.data.vertices]
        if not verts:
            continue
        xs = [v.x for v in verts]; ys=[v.y for v in verts]; zs=[v.z for v in verts]
        print(f"  {obj.name:30s} v={len(verts):6d}  "
              f"X=[{min(xs):+.3f},{max(xs):+.3f}] "
              f"Y=[{min(ys):+.3f},{max(ys):+.3f}] "
              f"Z=[{min(zs):+.3f},{max(zs):+.3f}]")
