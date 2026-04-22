"""What is the actual rest pose of the MPFB base mesh? Check bone head/tail
positions for the upperarm bones on the MPFB 53-bone rig."""
import os, sys, bpy
argv = sys.argv
gender = argv[argv.index("--") + 1] if "--" in argv else "male"
SRC = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\base_{gender}.blend"
bpy.ops.wm.open_mainfile(filepath=SRC)
arm = bpy.data.objects[f"base_{gender}.rig"]
print(f"MPFB rig bones:")
for bname in ["upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "hand_l", "thigh_l", "thigh_r", "pelvis", "spine_01", "spine_03"]:
    b = arm.data.bones.get(bname)
    if b:
        h = b.head_local
        t = b.tail_local
        # length/direction:
        import mathutils
        d = mathutils.Vector((t.x - h.x, t.y - h.y, t.z - h.z))
        print(f"  {bname:15s} head=({h.x:+.3f},{h.y:+.3f},{h.z:+.3f}) tail=({t.x:+.3f},{t.y:+.3f},{t.z:+.3f}) delta=({d.x:+.3f},{d.y:+.3f},{d.z:+.3f})")

# Mesh extent:
mesh = bpy.data.objects[f"base_{gender}"]
coords = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
xs = [c.x for c in coords]
ys = [c.y for c in coords]
zs = [c.z for c in coords]
print(f"Mesh bbox: X=[{min(xs):.3f},{max(xs):.3f}] Y=[{min(ys):.3f},{max(ys):.3f}] Z=[{min(zs):.3f},{max(zs):.3f}]")
print(f"Width (X span)={max(xs)-min(xs):.3f}, Depth (Y)={max(ys)-min(ys):.3f}, Height (Z)={max(zs)-min(zs):.3f}")
