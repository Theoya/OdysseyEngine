"""
Skin the MPFB2 base humanoid mesh to the OdysseyEngine 19-bone skeleton.

v2 (2026-04-21) — builds the engine armature at MPFB rest positions so the
mesh is undeformed at rest. Only bone NAMES use engine convention; positions
come from the actual MPFB body pose.

The walk cycle XML applies rotations in the bone's LOCAL frame, so as long
as we keep the MPFB local-axes for each bone, walk rotations behave sanely
on the underlying mesh. Result: armature runs the engine's walk_cycle.anim.xml
by name-match.

Trade-off vs v1: the engine 19-bone skeleton's Cartesian positions
(documented in `humanoid.skeleton.xml`) do not match 1:1 what's in the .blend
-- the engine skeleton is straight-limbed, MPFB is slightly splayed. This is
fine because the .blend is a TOOL-SIDE asset; engine runtime still uses the
primitive stick-figure renderer and does not load the .blend.

Usage:
  "<blender4.2>" --background --python skin_to_engine_skeleton.py -- <male|female>
"""

import os
import sys
import json

import bpy
from mathutils import Vector

argv = sys.argv
if "--" in argv:
    script_args = argv[argv.index("--") + 1 :]
else:
    script_args = []
if not script_args:
    raise SystemExit("usage: skin_to_engine_skeleton.py -- <male|female>")
gender = script_args[0]
assert gender in ("male", "female")

BASE_DIR = rf"T:\OdysseyEngine\third_party\base_humanoid\{gender}"
SRC_BLEND = os.path.join(BASE_DIR, f"base_{gender}.blend")
DST_BLEND = os.path.join(BASE_DIR, f"base_{gender}_skinned.blend")

MESH_NAME = f"base_{gender}"
MPFB_RIG_NAME = f"base_{gender}.rig"
ENG_RIG_NAME = f"base_{gender}.engine_rig"
ENG_ARMATURE_DATA_NAME = f"base_{gender}.engine_armature"

# MPFB bone name -> engine bone name. Also captures "take this MPFB bone's
# head/tail as the engine bone's rest geometry". Order below is
# parent-before-child so we can chain.
#
# For merged targets (chest gets spine_02+spine_03), we use the head of the
# FIRST source (spine_02) and the tail of the LAST source (spine_03).
#
# Fingers collapse into hand_* and inherit hand's geometry.
# Ball_l/r collapse into foot_l/r and inherit foot's geometry.
# Shoulders are synthesized as 5cm stubs at the clavicle head, pointing
# outward.
BONE_BUILD = [
    # (engine_name, parent, mpfb_head_bone, mpfb_tail_bone, or None for stub)
    ("root",         "",         "pelvis",      "pelvis"),
    ("spine",        "root",     "spine_01",    "spine_01"),
    ("chest",        "spine",    "spine_02",    "spine_03"),
    ("neck",         "chest",    "neck_01",     "neck_01"),
    ("head",         "neck",     "head",        "head"),
    ("shoulder_l",   "chest",    "clavicle_l",  "clavicle_l"),
    ("upper_arm_l",  "shoulder_l", "upperarm_l", "upperarm_l"),
    ("lower_arm_l",  "upper_arm_l", "lowerarm_l", "lowerarm_l"),
    ("hand_l",       "lower_arm_l", "hand_l",    "hand_l"),
    ("shoulder_r",   "chest",    "clavicle_r",  "clavicle_r"),
    ("upper_arm_r",  "shoulder_r", "upperarm_r", "upperarm_r"),
    ("lower_arm_r",  "upper_arm_r", "lowerarm_r", "lowerarm_r"),
    ("hand_r",       "lower_arm_r", "hand_r",    "hand_r"),
    ("upper_leg_l",  "root",     "thigh_l",     "thigh_l"),
    ("lower_leg_l",  "upper_leg_l", "calf_l",   "calf_l"),
    ("foot_l",       "lower_leg_l", "foot_l",   "foot_l"),
    ("upper_leg_r",  "root",     "thigh_r",     "thigh_r"),
    ("lower_leg_r",  "upper_leg_r", "calf_r",   "calf_r"),
    ("foot_r",       "lower_leg_r", "foot_r",   "foot_r"),
]

# Vertex-group merge table: MPFB sources -> engine destination.
MERGES = [
    (["pelvis"], "root"),
    (["spine_01"], "spine"),
    (["spine_02", "spine_03"], "chest"),
    (["clavicle_l"], "shoulder_l"),
    (["clavicle_r"], "shoulder_r"),
    (["neck_01"], "neck"),
    (["head"], "head"),
    (["upperarm_l"], "upper_arm_l"),
    (["upperarm_r"], "upper_arm_r"),
    (["lowerarm_l"], "lower_arm_l"),
    (["lowerarm_r"], "lower_arm_r"),
    ([
        "hand_l",
        "thumb_01_l", "thumb_02_l", "thumb_03_l",
        "index_01_l", "index_02_l", "index_03_l",
        "middle_01_l", "middle_02_l", "middle_03_l",
        "ring_01_l", "ring_02_l", "ring_03_l",
        "pinky_01_l", "pinky_02_l", "pinky_03_l",
    ], "hand_l"),
    ([
        "hand_r",
        "thumb_01_r", "thumb_02_r", "thumb_03_r",
        "index_01_r", "index_02_r", "index_03_r",
        "middle_01_r", "middle_02_r", "middle_03_r",
        "ring_01_r", "ring_02_r", "ring_03_r",
        "pinky_01_r", "pinky_02_r", "pinky_03_r",
    ], "hand_r"),
    (["thigh_l"], "upper_leg_l"),
    (["thigh_r"], "upper_leg_r"),
    (["calf_l"], "lower_leg_l"),
    (["calf_r"], "lower_leg_r"),
    (["foot_l", "ball_l"], "foot_l"),
    (["foot_r", "ball_r"], "foot_r"),
]

# --- open source ---
print(f"[{gender}] opening {SRC_BLEND}")
bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

mesh = bpy.data.objects.get(MESH_NAME)
if mesh is None:
    raise SystemExit(f"mesh {MESH_NAME} not found")
mpfb_rig = bpy.data.objects.get(MPFB_RIG_NAME)
if mpfb_rig is None:
    raise SystemExit(f"rig {MPFB_RIG_NAME} not found")

# --- capture MPFB bone head/tail world positions BEFORE we delete the rig ---
mpfb_bone_geom = {}
for b in mpfb_rig.data.bones:
    mpfb_bone_geom[b.name] = dict(
        head=mpfb_rig.matrix_world @ b.head_local,
        tail=mpfb_rig.matrix_world @ b.tail_local,
    )
print(f"[{gender}] captured {len(mpfb_bone_geom)} MPFB bone geometries")

# --- Remove any existing armature modifier on mesh. ---
for m in list(mesh.modifiers):
    if m.type == "ARMATURE":
        mesh.modifiers.remove(m)

# --- Merge vertex groups into engine-named groups. ---
def merge_vertex_groups(mesh_obj, source_names, dest_name):
    src_groups = [mesh_obj.vertex_groups.get(n) for n in source_names]
    src_groups = [g for g in src_groups if g is not None]
    src_indices = {g.index for g in src_groups}
    if not src_groups:
        return 0

    dest = mesh_obj.vertex_groups.get(dest_name)
    if dest is None:
        dest = mesh_obj.vertex_groups.new(name=dest_name)

    per_vertex = {}
    for v in mesh_obj.data.vertices:
        total = 0.0
        for g in v.groups:
            if g.group in src_indices:
                total += g.weight
        if total > 0.0:
            per_vertex[v.index] = total

    for idx, w in per_vertex.items():
        dest.add([idx], w, "REPLACE")

    for g in src_groups:
        if g.name != dest_name:
            mesh_obj.vertex_groups.remove(g)
    return len(per_vertex)


print(f"[{gender}] merging vertex groups...")
for src_names, dst in MERGES:
    n = merge_vertex_groups(mesh, src_names, dst)
    print(f"    {dst:15s} <- {len(src_names):2d} sources, {n:5d} vertices")

# --- Delete leftover vertex groups that aren't engine bone names. ---
engine_names = set(b[0] for b in BONE_BUILD)
for g in list(mesh.vertex_groups):
    if g.name not in engine_names:
        mesh.vertex_groups.remove(g)

# Ensure all 19 engine bone names have a vertex group (create empty if missing).
for name in engine_names:
    if mesh.vertex_groups.get(name) is None:
        mesh.vertex_groups.new(name=name)

print(f"[{gender}] final vertex groups: {len(mesh.vertex_groups)}")

# --- Remove MPFB armature. ---
bpy.data.objects.remove(mpfb_rig, do_unlink=True)
for a in list(bpy.data.armatures):
    if a.users == 0:
        bpy.data.armatures.remove(a)

# --- Build engine armature at MPFB rest positions. ---
print(f"[{gender}] building engine armature at MPFB rest positions")
arm_data = bpy.data.armatures.new(ENG_ARMATURE_DATA_NAME)
arm_obj = bpy.data.objects.new(ENG_RIG_NAME, arm_data)
bpy.context.scene.collection.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode="EDIT")

for eng_name, parent_name, mpfb_head_bone, mpfb_tail_bone in BONE_BUILD:
    head = mpfb_bone_geom[mpfb_head_bone]["head"]
    tail = mpfb_bone_geom[mpfb_tail_bone]["tail"]
    # For shoulder stubs (synthesized from clavicle), shorten to 5cm along
    # outward direction.
    if eng_name in ("shoulder_l", "shoulder_r"):
        direction = (tail - head)
        if direction.length > 0:
            direction.normalize()
        else:
            # Fallback: outward from spine.
            direction = Vector((-1.0 if eng_name.endswith("_l") else 1.0, 0, 0))
        tail = head + direction * 0.05
    # For root, ensure tail is ABOVE head (pelvis bone points UP in engine convention).
    if eng_name == "root":
        tail = head + Vector((0, 0, 0.10))

    eb = arm_data.edit_bones.new(eng_name)
    eb.head = head
    eb.tail = tail
    if parent_name:
        eb.parent = arm_data.edit_bones.get(parent_name)
        eb.use_connect = False

print(f"[{gender}] built {len(BONE_BUILD)} engine bones")

bpy.ops.object.mode_set(mode="OBJECT")

# --- Parent mesh to armature (ARMATURE_NAME: bind by vertex group name). ---
bpy.ops.object.select_all(action="DESELECT")
mesh.select_set(True)
arm_obj.select_set(True)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.parent_set(type="ARMATURE_NAME")

# Ensure armature modifier is present and pointing at arm_obj.
arm_mod = None
for m in mesh.modifiers:
    if m.type == "ARMATURE":
        arm_mod = m
        break
if arm_mod is None:
    arm_mod = mesh.modifiers.new("Armature", "ARMATURE")
arm_mod.object = arm_obj

# --- Normalize weights. ---
bpy.ops.object.select_all(action="DESELECT")
mesh.select_set(True)
bpy.context.view_layer.objects.active = mesh
bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
try:
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    print(f"    normalized vertex groups")
except Exception as e:
    print(f"    normalize failed: {e}")
bpy.ops.object.mode_set(mode="OBJECT")

# --- Report. ---
group_counts = {}
for g in mesh.vertex_groups:
    group_counts[g.name] = 0
for v in mesh.data.vertices:
    for vg in v.groups:
        if vg.weight > 0.0:
            gname = mesh.vertex_groups[vg.group].name
            group_counts[gname] = group_counts.get(gname, 0) + 1
for name in sorted(group_counts):
    print(f"    {name:15s} {group_counts[name]:6d} vertices")

driven_verts = 0
for v in mesh.data.vertices:
    total = sum(vg.weight for vg in v.groups)
    if 0.99 <= total <= 1.01:
        driven_verts += 1
print(f"[{gender}] driven verts (weight sum in [0.99,1.01]): {driven_verts}/{len(mesh.data.vertices)}")

# --- Save. ---
bpy.ops.wm.save_as_mainfile(filepath=DST_BLEND)
print(f"[{gender}] saved {DST_BLEND}")

stats = {
    "gender": gender,
    "engine_bones_built": len(BONE_BUILD),
    "vertex_groups_final": len(mesh.vertex_groups),
    "verts_total": len(mesh.data.vertices),
    "verts_driven_weight_sum_1": driven_verts,
    "per_group_vertex_counts": group_counts,
    "skin_approach": "v2_mpfb_rest_positions",
}
with open(os.path.join(BASE_DIR, f"skin_stats_{gender}.json"), "w") as f:
    json.dump(stats, f, indent=2)
print(f"--- skin v2 complete for {gender} ---")
