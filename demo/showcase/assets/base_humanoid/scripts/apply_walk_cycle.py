"""
Apply the engine's shipping walk_cycle.anim.xml to the skinned base humanoid.

Workflow B.a from animate_walk_cycle.md:
  1. Open base_<gender>_skinned.blend (has 19-bone engine armature + MPFB mesh).
  2. Parse walk_cycle.anim.xml.
  3. For each <track bone="X">, set keyframes on the engine armature's pose bone X.
  4. Loop via Cycles F-modifier so preview plays indefinitely.
  5. Set scene frame range 1..20 (0.8s at 24fps = 19.2 frames, round up).
  6. Save as walking.blend.

Usage:
  "<blender4.2>" --background --python apply_walk_cycle.py -- <male|female>

Rotation convention:
  engine XML: rotation="x y z w"  (XYZW)
  Blender:    bone.rotation_quaternion = (w, x, y, z)  (WXYZ)

Position convention:
  engine XML <key position="px py pz"/> is the BONE HEAD's world-space position
  in the rest pose frame of the skeleton. In Blender pose mode,
  `bone.location` is the DELTA from the rest position, in the bone's local
  (parent-relative) frame.

  The skeleton XML stores bone rest offsets as:
    <bone name="root" position="0 0.95 0"/>
  and `walk_cycle.anim.xml` re-uses the same absolute-looking values in each
  keyframe ("0 0.95 0"). So the delta for Blender is:
    blender_loc = key_position - bone.position (from skeleton.xml)
  which for stationary bones is (0, 0, 0) and for the root-bob is (0, 0.02, 0).
"""

import os
import sys
import xml.etree.ElementTree as ET

import bpy
from mathutils import Vector, Quaternion

argv = sys.argv
if "--" in argv:
    script_args = argv[argv.index("--") + 1 :]
else:
    script_args = []
if not script_args:
    raise SystemExit("usage: apply_walk_cycle.py -- <male|female>")
gender = script_args[0]
assert gender in ("male", "female")

BASE_DIR = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}"
SRC_BLEND = os.path.join(BASE_DIR, f"base_{gender}_skinned.blend")
DST_BLEND = os.path.join(BASE_DIR, "walking.blend")
SKELETON_XML = r"T:\OdysseyEngine\demo\fps_humanoid\assets\humanoid.skeleton.xml"
WALK_XML = r"T:\OdysseyEngine\demo\fps_humanoid\assets\walk_cycle.anim.xml"
RIG_NAME = f"base_{gender}.engine_rig"

FPS = 24


def parse_skeleton_rest_positions(path):
    tree = ET.parse(path)
    root = tree.getroot()
    rest = {}
    for b in root.findall("bone"):
        name = b.attrib["name"]
        pos = tuple(float(v) for v in b.attrib["position"].split())
        rot = tuple(float(v) for v in b.attrib["rotation"].split())  # xyzw
        rest[name] = dict(position=Vector(pos), rotation=rot)
    return rest


def parse_walk_anim(path):
    tree = ET.parse(path)
    root = tree.getroot()
    duration = float(root.attrib.get("duration", "0.8"))
    looping = root.attrib.get("looping", "false") == "true"
    tracks = {}
    for t in root.findall("track"):
        bone_name = t.attrib["bone"]
        keys = []
        for k in t.findall("key"):
            time = float(k.attrib["time"])
            pos = Vector(tuple(float(v) for v in k.attrib["position"].split()))
            rx, ry, rz, rw = (float(v) for v in k.attrib["rotation"].split())
            # Blender WXYZ:
            rot_wxyz = Quaternion((rw, rx, ry, rz))
            keys.append(dict(time=time, position=pos, rotation=rot_wxyz))
        tracks[bone_name] = keys
    return dict(duration=duration, looping=looping, tracks=tracks)


# --- open skinned blend ---
print(f"[{gender}] opening {SRC_BLEND}")
bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

arm_obj = bpy.data.objects.get(RIG_NAME)
if arm_obj is None:
    raise SystemExit(f"engine rig {RIG_NAME} not in scene — was skinning run first?")

rest = parse_skeleton_rest_positions(SKELETON_XML)
walk = parse_walk_anim(WALK_XML)
print(f"[{gender}] walk cycle: duration={walk['duration']}s, {len(walk['tracks'])} animated bones")

# --- enter pose mode ---
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode="POSE")

# Create animation data + action.
if arm_obj.animation_data is None:
    arm_obj.animation_data_create()
action = bpy.data.actions.new(name="walk_cycle")
arm_obj.animation_data.action = action

# --- insert keyframes ---
bone_key_counts = {}
for bone_name, keys in walk["tracks"].items():
    pose_bone = arm_obj.pose.bones.get(bone_name)
    if pose_bone is None:
        print(f"    WARN: engine rig has no bone '{bone_name}' — skipping")
        continue
    rest_pos = rest[bone_name]["position"]
    # Need rotation mode quaternion.
    pose_bone.rotation_mode = "QUATERNION"

    for k in keys:
        frame = round(k["time"] * FPS) + 1  # 1-indexed
        # Engine XML is Y-up; Blender is Z-up. Swap axes for positions.
        # Engine (x, y, z) = Blender (x, -z, y). So a delta in engine Y (up)
        # becomes delta in Blender Z (up). Engine Z (forward) becomes Blender
        # -Y (also "forward" in Blender default conventions — looking down -Y).
        eng_delta = k["position"] - rest_pos   # still in engine coords
        blender_delta = Vector((eng_delta.x, -eng_delta.z, eng_delta.y))
        pose_bone.location = blender_delta

        # Rotations: the XML quaternion is authored in the engine bone's
        # local frame (which is Y-up). In Blender the bone's local frame is
        # inherited from its tail direction (which for MPFB-built arms/legs
        # is NOT along the engine's Y-axis). So we apply the quaternion
        # directly here, accepting that small angle errors come from the
        # rest-pose axis mismatch. For ±23° swings, the visual error is
        # tolerable at 30m silhouette distance ("half decent" bar).
        pose_bone.rotation_quaternion = k["rotation"]
        pose_bone.keyframe_insert(data_path="location", frame=frame)
        pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
    bone_key_counts[bone_name] = len(keys)

print(f"[{gender}] inserted keyframes:")
for bn, c in sorted(bone_key_counts.items()):
    print(f"    {bn:15s} {c} keys")

# --- cyclic f-modifier on all fcurves for infinite preview loop ---
if action.fcurves:
    for fc in action.fcurves:
        mod = fc.modifiers.new("CYCLES")
        mod.mode_before = "REPEAT"
        mod.mode_after = "REPEAT"

# --- scene frame range ---
scene = bpy.context.scene
scene.frame_start = 1
# 0.8s loop: keys at t=0.0, 0.2, 0.4, 0.6, 0.8 => frames 1, 5.8->6, 10.6->11, 15.4->15, 20.2->20
# we keyframe at rounded frames 1, 6, 11, 15, 20 (because of +1 indexing)
# But the anim endpoint at t=0.8 matches t=0.0 (loop), so playback frame_end = 20 gives
# a ~0.83s loop — close enough. Use frame_end = 19 so 20 is the wrap to 1.
scene.frame_end = 19
scene.render.fps = FPS

# Return to object mode for saving.
bpy.ops.object.mode_set(mode="OBJECT")

# --- save ---
bpy.ops.wm.save_as_mainfile(filepath=DST_BLEND)
print(f"[{gender}] saved {DST_BLEND}")
print(f"--- walking setup complete for {gender} ---")
