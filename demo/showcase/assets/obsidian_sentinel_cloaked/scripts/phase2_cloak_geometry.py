"""
Phase 2 — Model the cloak geometry onto the Obsidian Sentinel.

Builds a cape mesh as a parameterized open cylinder:
  - pinned at shoulder/neck height (ring at Z ≈ 1.40m)
  - drapes to ankle (ring bottom at Z ≈ 0.06m)
  - open in the FRONT (U-shape: ~270° of arc behind the figure)
  - parallelogram-grid quads, even tessellation (cloth solvers need this)
  - slight initial outward bulge so the first cloth frame isn't clipping through the body

Additionally:
  - defines a vertex group `pin_ring` = the TOP RING of verts + shoulder saddle
  - adds a tall collar (stand-up neck ring) rather than a full hood, to avoid
    colliding with the character's helmet horns (documented in skill guide)

Output:
  - T:/OdysseyEngine/third_party/obsidian_sentinel_cloaked/cloak.blend
    (contains base character + cloak mesh, both in Z-up, ready for Phase 3)

Run:
  "C:\\Users\\THadfield\\Blender 4.2\\blender-4.2.20-windows-x64\\blender.exe" \
      --background --python phase2_cloak_geometry.py
"""

import bpy
import bmesh
import os
import math
import mathutils

# ----------------------------- paths -----------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", "..", "..", ".."))
SOURCE_DIR = os.path.join(
    PROJECT_ROOT, "third_party", "meshy_imports", "obsidian_sentinel",
    "Meshy_AI_Obsidian_Sentinel_biped",
)
OUT_DIR = os.path.join(PROJECT_ROOT, "third_party", "obsidian_sentinel_cloaked")
os.makedirs(OUT_DIR, exist_ok=True)

CHARACTER_GLB = os.path.join(
    SOURCE_DIR,
    "Meshy_AI_Obsidian_Sentinel_biped_Character_output.glb",
)
WALKING_GLB = os.path.join(
    SOURCE_DIR,
    "Meshy_AI_Obsidian_Sentinel_biped_Animation_Walking_withSkin.glb",
)

# ----------------------------- reset scene -----------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)

# ========================================================
# Step 1 — Import character + graft Walking action
# ========================================================
print("=" * 70); print("PHASE 2: Import character + graft Walking action"); print("=" * 70)

bpy.ops.import_scene.gltf(filepath=CHARACTER_GLB)

# Remove the stray Icosphere that comes with this Meshy export
for name in list(bpy.data.objects):
    if "Icosphere" in name:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)

base_arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
base_meshes = [o for o in bpy.data.objects if o.type == "MESH"]
assert base_arms, "No armature imported from character glb"
assert base_meshes, "No mesh imported from character glb"
arm = base_arms[0]
body = base_meshes[0]
print(f"Character: armature={arm.name} mesh={body.name} verts={len(body.data.vertices)}")

# Graft Walking action
pre_actions = set(a.name for a in bpy.data.actions)
pre_objects = set(o.name for o in bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=WALKING_GLB)
new_actions = set(a.name for a in bpy.data.actions) - pre_actions
new_objects = set(o.name for o in bpy.data.objects) - pre_objects

walk_action_name = next(iter(new_actions)) if new_actions else None
assert walk_action_name, "No new action came from the Walking glb"
arm.animation_data_create()
arm.animation_data.action = bpy.data.actions[walk_action_name]

# Delete the objects imported from the Walking glb
for name in new_objects:
    if name in bpy.data.objects:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)
print(f"Walking action grafted: {walk_action_name}")

# Frame range
act = bpy.data.actions[walk_action_name]
bpy.context.scene.frame_start = int(act.frame_range[0])
bpy.context.scene.frame_end = int(act.frame_range[1])
bpy.context.scene.frame_set(bpy.context.scene.frame_start)

# Make sure we're in object mode at rest frame
bpy.context.view_layer.update()

# ========================================================
# Step 2 — Read shoulder/neck attachment points in WORLD space
# ========================================================
print()
print("=" * 70); print("Reading attachment-ring world positions"); print("=" * 70)

# In this rig the armature has a global transform that scales cm → m (0.01).
# bone.head_local is in ARMATURE LOCAL coordinates (cm). Transform to world:
arm_mw = arm.matrix_world

def bone_head_world(name):
    b = arm.data.bones[name]
    return arm_mw @ b.head_local

pin_bones = {
    "neck":          bone_head_world("neck"),
    "spine":         bone_head_world("Spine"),
    "left_shldr":    bone_head_world("LeftShoulder"),
    "right_shldr":   bone_head_world("RightShoulder"),
    "left_arm":      bone_head_world("LeftArm"),
    "right_arm":     bone_head_world("RightArm"),
}
for k, v in pin_bones.items():
    print(f"  {k}: ({v.x:.3f}, {v.y:.3f}, {v.z:.3f})")

# Figure height & cape geometry
body_bbox_world_lo = body.matrix_world @ mathutils.Vector(body.bound_box[0])
body_bbox_world_hi = body.matrix_world @ mathutils.Vector(body.bound_box[6])
for c in body.bound_box:
    p = body.matrix_world @ mathutils.Vector(c)
    body_bbox_world_lo.x = min(body_bbox_world_lo.x, p.x)
    body_bbox_world_lo.y = min(body_bbox_world_lo.y, p.y)
    body_bbox_world_lo.z = min(body_bbox_world_lo.z, p.z)
    body_bbox_world_hi.x = max(body_bbox_world_hi.x, p.x)
    body_bbox_world_hi.y = max(body_bbox_world_hi.y, p.y)
    body_bbox_world_hi.z = max(body_bbox_world_hi.z, p.z)
print(f"Body world bbox: min=({body_bbox_world_lo.x:.3f}, {body_bbox_world_lo.y:.3f}, "
      f"{body_bbox_world_lo.z:.3f}) max=({body_bbox_world_hi.x:.3f}, {body_bbox_world_hi.y:.3f}, "
      f"{body_bbox_world_hi.z:.3f})")

# ========================================================
# Step 3 — Build cloak mesh procedurally
# ========================================================
print()
print("=" * 70); print("Building cloak mesh"); print("=" * 70)

# Parameters chosen to meet the ~1000-vert / quads-only mandate with even tess.
# - 26 rings top-to-bottom (=25 horizontal quad rows)
# - 34 arc segments from one shoulder around the back to the other (=33 quad cols)
# - => 26 * 34 = 884 verts (<1000), 25 * 33 = 825 quads, pure quads.
N_RINGS = 26
N_ARCS = 34          # horizontal segments along the arc (exclusive of endpoints)
ARC_DEG = 250.0      # arc opens 360-250=110° across the front (like a cape)
TOP_Z = pin_bones["neck"].z - 0.02   # collar slightly below neck base
BOTTOM_Z = 0.06                      # just above floor to avoid immediate ground collide
TOP_RADIUS = 0.23                    # shoulder ring radius
BOTTOM_RADIUS = 0.55                 # hem flares outward
COLLAR_LIFT = 0.10                   # extra vertical rise on top ring (suggests hood)
BACKWARD_Y_OFFSET = -0.08            # bulge the cape backward so it starts behind the body

# Cape is mirrored left-right about the figure's center
arc_rad = math.radians(ARC_DEG)
# center the arc so the OPENING is at the FRONT (positive local X direction of figure?)
# This Meshy rig's FORWARD is +Y in world-space (character glb imports +Y-up oriented)
# In our case: character faces +Y (see standard glTF import convention).
# Actually: the glTF import translates Y-up to Z-up by rotating -90° X, so the character
# ends up facing -Y in Blender. Quick empirical approach: open arc centered on -Y.
# The cape arc will go from start_angle to end_angle such that the MIDPOINT of the
# arc points backward. "Backward" (behind the character) is +Y if character faces -Y.

# Build ring positions
verts = []
vert_row_col_to_idx = {}
for i in range(N_RINGS):
    t = i / (N_RINGS - 1)            # 0 at top, 1 at bottom
    # Smooth profile — tapered up slightly, flared at hem
    r = TOP_RADIUS + (BOTTOM_RADIUS - TOP_RADIUS) * (t ** 1.2)
    z = TOP_Z * (1 - t) + BOTTOM_Z * t
    # Add collar lift at the top (invert easing: only top few rings)
    if i < 3:
        z += COLLAR_LIFT * (1 - (i / 2.0))
    # Add slight backward bulge that fades out at the hem (so it doesn't flap behind feet)
    back_offset_falloff = (1 - t) ** 0.7
    y_bulge = BACKWARD_Y_OFFSET * back_offset_falloff
    for j in range(N_ARCS):
        u = j / (N_ARCS - 1)         # 0 = left-shoulder pin, 1 = right-shoulder pin
        # angle: start at pi/2 + arc_rad/2, sweep to pi/2 - arc_rad/2 (going through pi, i.e. +Y-backward)
        # Actually the convention: we want the arc to wrap AROUND the BACK of the figure.
        # If character faces -Y, then +Y is BEHIND the character.
        # Parameterise angle so that u=0 is at -X (left shoulder front-left), u=0.5 is at +Y (straight behind), u=1 is at +X.
        # Starting angle: at u=0, angle = pi (negative X-axis direction rotated... hmm)
        # Simpler: define polar in (x,y): start_phi = -90° - ARC_DEG/2, sweep clockwise through
        # +90° (behind = +Y) and end at -90° + ARC_DEG/2. Convert: phi(u) = (-90° - ARC_DEG/2) + u*ARC_DEG.
        # Hmm. Use angle measured from +X counter-clockwise.
        # We want:
        #   u=0   → angle such that x,y = (-TOP_RADIUS * something, 0 or +Y bulge)  (left shoulder-ish)
        #   u=0.5 → angle points to +Y (back)
        #   u=1   → right shoulder-ish
        # So angle(u) goes from (90° + ARC_DEG/2) CCW-wise... no CW.
        # Let's use: angle = 90° + (u - 0.5) * ARC_DEG.
        # At u=0: angle = 90 - ARC/2.  At u=0.5: angle = 90.  At u=1: angle = 90 + ARC/2.
        # With ARC=250:
        #   u=0  : angle = 90 - 125 = -35°  → x=r*cos(-35°)=0.82r, y=r*sin(-35°)=-0.57r → front-right
        #   u=0.5: angle = 90°              → (0, r) → straight behind  (+Y)
        #   u=1  : angle = 90 + 125 = 215°  → x=r*cos(215°)=-0.82r, y=r*sin(215°)=-0.57r → front-left
        # So u goes right-front → back → left-front. Flip so u=0 is LEFT-front.
        phi_deg = 90.0 - (u - 0.5) * ARC_DEG
        phi = math.radians(phi_deg)
        x = r * math.cos(phi)
        y = r * math.sin(phi) + y_bulge
        verts.append((x, y, z))
        vert_row_col_to_idx[(i, j)] = len(verts) - 1

# Build faces
faces = []
for i in range(N_RINGS - 1):
    for j in range(N_ARCS - 1):
        a = vert_row_col_to_idx[(i, j)]
        b = vert_row_col_to_idx[(i, j + 1)]
        c = vert_row_col_to_idx[(i + 1, j + 1)]
        d = vert_row_col_to_idx[(i + 1, j)]
        faces.append((a, b, c, d))

print(f"Cloak verts: {len(verts)}  quads: {len(faces)}")

# Create mesh datablock
mesh = bpy.data.meshes.new("cloak_mesh")
mesh.from_pydata(verts, [], faces)
mesh.update()

cloak = bpy.data.objects.new("cloak", mesh)
bpy.context.collection.objects.link(cloak)

# ========================================================
# Step 4 — Pin vertex group (top ring — these stick to the character)
# ========================================================
print()
print("=" * 70); print("Defining pin vertex group"); print("=" * 70)

vg_pin = cloak.vertex_groups.new(name="pin_ring")
# top ring = row 0
pin_idx = [vert_row_col_to_idx[(0, j)] for j in range(N_ARCS)]
# Also ring 1 for a more gradual pin band (half weight)
pin_band_idx = [vert_row_col_to_idx[(1, j)] for j in range(N_ARCS)]
vg_pin.add(pin_idx, 1.0, "REPLACE")
vg_pin.add(pin_band_idx, 0.4, "REPLACE")
print(f"Pin ring: {len(pin_idx)} verts fully pinned, {len(pin_band_idx)} verts 0.4-weighted")

# ========================================================
# Step 5 — Parent cloak to armature via Spine bone (so pin ring moves with torso)
# ========================================================
# The cloak's pinned verts need to follow the character's shoulder ring every frame.
# Approach: parent the cloak object to the SPINE/CHEST bone via Bone Parenting.
# Cloth sim's "Pin Group" then constrains those verts to follow the parent.
print()
print("=" * 70); print("Parenting cloak to Spine bone"); print("=" * 70)

# Select the armature as active
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm

# Enter pose mode, pick the bone
bpy.ops.object.mode_set(mode="POSE")
# Spine01 is the CHEST-level spine bone (Z ≈ 120cm = 1.20m in local)
# Actually we want the shoulder ring to drive this, so Spine02 is even better (Z ≈ 108).
# Pick the one closest to the pin-ring height (1.40m world -> bone at local Z ≈ 140).
# In local cm: neck = 139, Spine02 = 108, Spine01 = 120, Spine = 131.
# Closest is 'Spine' at local Z=131cm = 1.31m world. Use that.
arm.data.bones.active = arm.data.bones["Spine"]

bpy.ops.object.mode_set(mode="OBJECT")

# Now parent the cloak to the selected bone
bpy.ops.object.select_all(action="DESELECT")
cloak.select_set(True)
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type="BONE", keep_transform=True)
print(f"Cloak parented to bone 'Spine' (keep_transform=True)")

# ========================================================
# Step 6 — Save blend (pre-cloth-sim)
# ========================================================
out_blend = os.path.join(OUT_DIR, "cloak.blend")
bpy.ops.wm.save_as_mainfile(filepath=out_blend)
print(f"Saved: {out_blend}")

# Summary
total_verts = sum(len(o.data.vertices) for o in bpy.data.objects if o.type == "MESH")
total_polys = sum(len(o.data.polygons) for o in bpy.data.objects if o.type == "MESH")
print()
print(f"Scene totals: verts={total_verts}, polys={total_polys}")
print(f"Cloak-only: verts={len(verts)}, quads={len(faces)}")
print()
print("=" * 70); print("PHASE 2 COMPLETE"); print("=" * 70)
