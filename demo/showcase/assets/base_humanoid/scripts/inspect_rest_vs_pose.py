"""Diagnostic: load walking.blend, frame-step, dump bone rotations and the
actual world-space bone orientations.

This tells us if the XML rotations (XYZW) are being applied as expected.
"""
import os, sys
import bpy
from mathutils import Vector, Quaternion

argv = sys.argv
gender = argv[argv.index("--") + 1] if "--" in argv else "male"
SRC = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}\walking.blend"
bpy.ops.wm.open_mainfile(filepath=SRC)

arm = bpy.data.objects[f"base_{gender}.engine_rig"]
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="POSE")

for frame_want in [1, 6, 11, 15, 20]:
    bpy.context.scene.frame_set(frame_want)
    bpy.context.view_layer.update()
    print(f"--- frame {frame_want} ---")
    for bname in ["upper_arm_r", "upper_arm_l", "upper_leg_r", "upper_leg_l", "root"]:
        pb = arm.pose.bones.get(bname)
        if pb is None: continue
        q = pb.rotation_quaternion
        loc = pb.location
        # World-space head/tail:
        world_head = arm.matrix_world @ pb.head
        world_tail = arm.matrix_world @ pb.tail
        delta = world_tail - world_head
        print(f"  {bname:15s} rot_wxyz=({q.w:+.3f},{q.x:+.3f},{q.y:+.3f},{q.z:+.3f})  loc=({loc.x:+.3f},{loc.y:+.3f},{loc.z:+.3f})  tail_delta=({delta.x:+.3f},{delta.y:+.3f},{delta.z:+.3f})")
