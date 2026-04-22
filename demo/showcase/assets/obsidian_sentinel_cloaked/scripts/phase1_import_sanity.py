"""
Phase 1 — Import sanity check for Meshy Obsidian Sentinel biped.

Loads:
  - Character_output.glb   (rigged base)
  - Walking_withSkin.glb   (skin-baked anim; primary pick)
  - Casual_Walk_withSkin.glb (alternative; for comparison)

Verifies:
  - armature + skinned mesh import
  - animation has keyframes and plays through without NaN
  - prints bone names & bounding box so Phase 2/3 can reference them

Output:
  - prints a JSON-ish diagnostic report to stdout
  - writes phase1_sanity.png screenshot (Eevee render, T-pose-ish)
  - writes phase1_sanity.blend for inspection

Run:
  "C:\\Users\\THadfield\\Blender 4.2\\blender-4.2.20-windows-x64\\blender.exe" \
      --background --python phase1_import_sanity.py
"""

import bpy
import os
import sys
import math
import mathutils

# ----------------------------- paths -----------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "..", "..", ".."))
SOURCE_DIR = os.path.join(
    PROJECT_ROOT, "third_party", "meshy_imports", "obsidian_sentinel",
    "Meshy_AI_Obsidian_Sentinel_biped",
)
OUT_DIR = os.path.join(PROJECT_ROOT, "demo", "showcase", "assets",
                       "obsidian_sentinel_cloaked")
os.makedirs(OUT_DIR, exist_ok=True)

CHARACTER_GLB = os.path.join(
    SOURCE_DIR,
    "Meshy_AI_Obsidian_Sentinel_biped_Character_output.glb",
)
WALKING_GLB = os.path.join(
    SOURCE_DIR,
    "Meshy_AI_Obsidian_Sentinel_biped_Animation_Walking_withSkin.glb",
)
CASUAL_WALK_GLB = os.path.join(
    SOURCE_DIR,
    "Meshy_AI_Obsidian_Sentinel_biped_Animation_Casual_Walk_withSkin.glb",
)

# ----------------------------- reset scene -----------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)

# ============================================================
# 1) Inspect CHARACTER_OUTPUT
# ============================================================
print("=" * 70)
print("PHASE 1: Inspecting Character_output.glb")
print("=" * 70)

bpy.ops.import_scene.gltf(filepath=CHARACTER_GLB)

armatures = [o for o in bpy.data.objects if o.type == "ARMATURE"]
meshes = [o for o in bpy.data.objects if o.type == "MESH"]

print(f"Armatures imported: {len(armatures)}")
for a in armatures:
    print(f"  - {a.name}  bones={len(a.data.bones)}")
    print(f"    bone names (first 20):")
    for b in list(a.data.bones)[:20]:
        print(f"      {b.name}  head={tuple(round(v,3) for v in b.head_local)}"
              f"  tail={tuple(round(v,3) for v in b.tail_local)}")
    if len(a.data.bones) > 20:
        print(f"      ...({len(a.data.bones) - 20} more)")

print(f"Meshes imported: {len(meshes)}")
for m in meshes:
    bb = [mathutils.Vector(c) for c in m.bound_box]
    mn = mathutils.Vector((min(v.x for v in bb),
                           min(v.y for v in bb),
                           min(v.z for v in bb)))
    mx = mathutils.Vector((max(v.x for v in bb),
                           max(v.y for v in bb),
                           max(v.z for v in bb)))
    print(f"  - {m.name}  verts={len(m.data.vertices)}  polys={len(m.data.polygons)}")
    print(f"    bbox_local min={tuple(round(v,3) for v in mn)}"
          f"  max={tuple(round(v,3) for v in mx)}")
    # world-space height
    world_lo = m.matrix_world @ mn
    world_hi = m.matrix_world @ mx
    print(f"    world_bbox min={tuple(round(v,3) for v in world_lo)}"
          f"  max={tuple(round(v,3) for v in world_hi)}")

# Identify shoulder / neck attachment bones for cloak pinning (Phase 2)
if armatures:
    arm = armatures[0]
    candidates = ["neck", "head", "chest", "spine", "shoulder", "clavicle",
                  "collar", "upper_chest", "torso", "upperarm"]
    print("\nAttachment bone candidates (for cloak pin ring):")
    for b in arm.data.bones:
        low = b.name.lower()
        if any(c in low for c in candidates):
            print(f"  head_local={tuple(round(v,3) for v in b.head_local)}"
                  f"  tail_local={tuple(round(v,3) for v in b.tail_local)}"
                  f"  name={b.name}")

# ============================================================
# 2) Inspect WALKING (primary candidate)
# ============================================================
print()
print("=" * 70)
print("PHASE 1: Inspecting Walking_withSkin.glb")
print("=" * 70)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=WALKING_GLB)

walk_arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
walk_actions = list(bpy.data.actions)
print(f"Actions present: {len(walk_actions)}")
for a in walk_actions:
    fcs = len(a.fcurves)
    fr = a.frame_range
    print(f"  - {a.name}  fcurves={fcs}  frame_range=({fr[0]}, {fr[1]})")

# Sample the action at a few frames, check for NaN
if walk_arms and walk_actions:
    arm = walk_arms[0]
    act = walk_actions[0]
    arm.animation_data_create()
    arm.animation_data.action = act
    fstart, fend = int(act.frame_range[0]), int(act.frame_range[1])
    print(f"  Sampling frames {fstart}..{fend} step 5 for NaN...")
    nan_found = False
    for f in range(fstart, fend + 1, 5):
        bpy.context.scene.frame_set(f)
        bpy.context.view_layer.update()
        for b in arm.pose.bones:
            m = b.matrix
            for row in m:
                for v in row:
                    if not (v == v):  # NaN check
                        nan_found = True
                        print(f"    NaN at frame {f} bone {b.name}")
                        break
    print(f"  NaN check: {'FAIL' if nan_found else 'PASS'}")

# ============================================================
# 3) Inspect CASUAL_WALK (alternative)
# ============================================================
print()
print("=" * 70)
print("PHASE 1: Inspecting Casual_Walk_withSkin.glb (comparison)")
print("=" * 70)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=CASUAL_WALK_GLB)
cw_actions = list(bpy.data.actions)
for a in cw_actions:
    fr = a.frame_range
    print(f"  - {a.name}  fcurves={len(a.fcurves)}  frame_range=({fr[0]}, {fr[1]})")

# ============================================================
# 4) Build a sanity-check blend: character_output + Walking action grafted
# ============================================================
print()
print("=" * 70)
print("PHASE 1: Building sanity-check blend with Walking action grafted")
print("=" * 70)

bpy.ops.wm.read_factory_settings(use_empty=True)
# Import character (base rig + mesh)
bpy.ops.import_scene.gltf(filepath=CHARACTER_GLB)
base_arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
base_meshes = [o for o in bpy.data.objects if o.type == "MESH"]
print(f"Base: armatures={len(base_arms)} meshes={len(base_meshes)}")

# Import Walking as a separate collection so we can grab its action
# Strategy: import into the same scene; the Walking glb brings its own rig+mesh
# and its own Action. We copy the Action over to the base rig, then delete
# Walking's rig+mesh.
pre_action_names = set(a.name for a in bpy.data.actions)
pre_objects = set(o.name for o in bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=WALKING_GLB)
post_actions = set(a.name for a in bpy.data.actions) - pre_action_names
post_objects = set(o.name for o in bpy.data.objects) - pre_objects

print(f"Imported from Walking: objects={len(post_objects)} actions={len(post_actions)}")
walk_action_name = next(iter(post_actions)) if post_actions else None
if walk_action_name:
    print(f"Walking action name: {walk_action_name}")
    # Attach the walking action to the base armature
    base_arm = base_arms[0]
    base_arm.animation_data_create()
    base_arm.animation_data.action = bpy.data.actions[walk_action_name]

# Remove the imported-from-Walking objects (keep the base rig+mesh)
for name in post_objects:
    if name in bpy.data.objects:
        ob = bpy.data.objects[name]
        bpy.data.objects.remove(ob, do_unlink=True)

# Set frame range
if walk_action_name:
    act = bpy.data.actions[walk_action_name]
    bpy.context.scene.frame_start = int(act.frame_range[0])
    bpy.context.scene.frame_end = int(act.frame_range[1])
    bpy.context.scene.frame_set(int(act.frame_range[0]) + (int(act.frame_range[1]) - int(act.frame_range[0])) // 2)

# Save sanity blend
sanity_blend = os.path.join(OUT_DIR, "phase1_sanity.blend")
bpy.ops.wm.save_as_mainfile(filepath=sanity_blend)
print(f"Saved: {sanity_blend}")

# ============================================================
# 5) Render a sanity screenshot (Eevee)
# ============================================================
print()
print("=" * 70)
print("PHASE 1: Rendering sanity screenshot")
print("=" * 70)

scn = bpy.context.scene

# Simple lighting
bpy.ops.object.light_add(type="SUN", location=(4, -4, 6))
sun = bpy.context.object
sun.data.energy = 4.0
sun.rotation_euler = (math.radians(55), math.radians(20), math.radians(30))

# Key light
bpy.ops.object.light_add(type="AREA", location=(3, -4, 2.5))
key = bpy.context.object
key.data.energy = 200.0
key.data.size = 3.0
key.rotation_euler = (math.radians(70), 0, math.radians(40))

# Camera
bpy.ops.object.camera_add(location=(3.5, -3.5, 1.6))
cam = bpy.context.object
cam.data.lens = 70
# Point at origin Y=1 (mid-torso of ~1.8m figure)
direction = mathutils.Vector((0, 0, 1.0)) - cam.location
cam.rotation_mode = "QUATERNION"
cam.rotation_quaternion = direction.to_track_quat("-Z", "Y")
scn.camera = cam

# World background
if scn.world is None:
    scn.world = bpy.data.worlds.new("World")
scn.world.use_nodes = True
bg = scn.world.node_tree.nodes.get("Background")
if bg:
    bg.inputs[0].default_value = (0.02, 0.02, 0.03, 1.0)
    bg.inputs[1].default_value = 1.0

# Eevee settings (Blender 4.2 — Eevee-Next uses 'BLENDER_EEVEE_NEXT')
scn.render.engine = "BLENDER_EEVEE_NEXT"
scn.eevee.taa_render_samples = 32
scn.view_settings.view_transform = "Filmic"
scn.view_settings.look = "High Contrast"
scn.render.resolution_x = 960
scn.render.resolution_y = 540
scn.render.resolution_percentage = 100
scn.render.image_settings.file_format = "PNG"
scn.render.filepath = os.path.join(OUT_DIR, "phase1_sanity.png")

bpy.ops.render.render(write_still=True)
print(f"Saved: {scn.render.filepath}")

print()
print("=" * 70)
print("PHASE 1 COMPLETE")
print("=" * 70)
