"""
Torso armor — layered brutal sci-fi carapace on the male base humanoid.
ITERATION 2 (2026-04-21): fixes from visual-QA review (main-thread Claude).

Iter-1 defects (with fixes applied here):
  1. Armor offset to viewer's right  -> sternum_ridge placed FIRST at X=0,
     all chevron plates authored as MIRRORED L/R pairs (not single wide plates).
  2. Back completely unarmored        -> add 7 vertebra boxes + 2 scapula
     trapezoids + 1 lumbar band (full back carapace).
  3. Body rendered pure white         -> undersuit material darkened to
     (0.08, 0.08, 0.10) matte to contrast armor value.
  4. Armor footprint too small (~25%) -> chest chevrons widened to 38/34/30cm,
     ribs widened to 28->20cm taper, sternum runs FULL chest-to-navel (~60cm).
  5. Chevrons read as horizontal strips -> each L/R plate yawed +/-18deg about
     Z so inner edges meet at sternum and outer edges sweep down/outward.

Pieces (iter-2):
  A.  torso_sternum                    — 4cm wide full-chest spine (X=0, ~60cm tall)
  B.  torso_chevron_{01,02,03}_{l,r}   — 6 mirrored chevron halves (top/mid/low x L/R)
  C.  torso_rib_{01..05}               — 5 horizontal abdominal ribs (centered X=0, wider)
  D.  torso_vertebra_{01..07}          — 7 small spine boxes down centerline of back
  E.  torso_scapula_{l,r}              — 2 trapezoid plates on shoulder blades
  F.  torso_lumbar                     — 1 ribbed lumbar band low-back

Per skill guide gotchas: wide plates shrinkwrap onto shoulders if wider than
pec; we solve by authoring plates as L/R halves, NEAREST_SURFACEPOINT per
half (half-pec-width <= ~19cm fits one pec comfortably). Back plates use
PROJECT along -Y with cull_face=FRONT so rays hit the back surface from
behind (verify +Z front-vs-back at runtime).

Usage (Blender 4.2 portable):
  "C:\\Users\\THadfield\\Blender 4.2\\blender-4.2.20-windows-x64\\blender.exe" \
    --background --python sculpt_torso_armor_male.py

Outputs:
  T:\\OdysseyEngine\\third_party\\base_humanoid\\male\\torso_armor.blend
  T:\\OdysseyEngine\\third_party\\base_humanoid\\male\\torso_armor.obj
"""

import os
import sys
import math

import bpy
import bmesh
from mathutils import Vector, Matrix

OUT_DIR = r"T:\OdysseyEngine\third_party\base_humanoid\male"
SRC_BLEND = os.path.join(OUT_DIR, "base_male.blend")
DST_BLEND = os.path.join(OUT_DIR, "torso_armor.blend")
DST_OBJ   = os.path.join(OUT_DIR, "torso_armor.obj")

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

basemesh = bpy.data.objects["base_male"]
bpy.context.view_layer.update()

# --- 1. body landmarks from world-space bounding box ---
world = basemesh.matrix_world
coords = [world @ v.co for v in basemesh.data.vertices]
min_z = min(c.z for c in coords); max_z = max(c.z for c in coords)
min_y = min(c.y for c in coords); max_y = max(c.y for c in coords)
min_x = min(c.x for c in coords); max_x = max(c.x for c in coords)
body_h = max_z - min_z
body_depth = max_y - min_y   # Y is forward/back (Blender convention here: -Y front, +Y back)

# Z landmarks along body height. Expanded coverage vs iter-1: sternum now
# runs from collarbone notch (0.755) down through navel (0.58) -> ~60cm tall.
Z_NAVEL        = min_z + 0.58  * body_h
Z_LOW_ABDOMEN  = min_z + 0.605 * body_h
Z_DIAPHRAGM    = min_z + 0.640 * body_h
Z_LOW_CHEST    = min_z + 0.668 * body_h
Z_MID_CHEST    = min_z + 0.694 * body_h
# Iter-2 fix: cap chevron top at 0.715 (not 0.730) to account for yaw+shrinkwrap
# adding ~3cm Z-extent at corners. Previous 0.730 produced verts up to Z=1.491m
# which is 4.7cm ABOVE the collarbone (1.444m) — visible as chevrons creeping
# into neck/clavicle region.
Z_UPPER_CHEST  = min_z + 0.715 * body_h
Z_COLLAR       = min_z + 0.755 * body_h
# Back-plate Z bands (mirror front for carapace continuity).
Z_SHOULDER_BLADE_HI = min_z + 0.735 * body_h
Z_SHOULDER_BLADE_LO = min_z + 0.670 * body_h
Z_LUMBAR       = min_z + 0.610 * body_h
# Back spine range: top of back (just below neck) to sacrum/lower-back.
Z_SPINE_TOP    = min_z + 0.745 * body_h
Z_SPINE_BOT    = min_z + 0.585 * body_h

# Forward standoff: -Y is front (chest side).
FRONT_Y = -0.30
# Back standoff: +Y is back side. Iter-2: moved from +0.30 to +0.22 so the
# PROJECT shrinkwrap's default 0.30m project_limit easily reaches the upper
# spine (body back Y ≈ +0.05 at Z=1.40). Previous +0.30 + project_limit=0.20
# meant rays couldn't reach the upper spine band → all back-plate verts
# stayed at spawn Y → trim_floaters deleted every vert.
BACK_Y  =  0.22

print(f"[torso_armor] body height {body_h:.3f}m  min_z={min_z:.3f}  max_z={max_z:.3f}")
print(f"             body Y range: [{min_y:.3f}, {max_y:.3f}]  body_depth={body_depth:.3f}")
print(f"             front chest Z: navel={Z_NAVEL:.3f} diaphragm={Z_DIAPHRAGM:.3f} "
      f"mid={Z_MID_CHEST:.3f} upper={Z_UPPER_CHEST:.3f} collar={Z_COLLAR:.3f}")


# --- shared helper: build a planar armor plate ---
def make_plate(
    name,
    location,
    width,      # x-extent (m)
    height,     # z-extent (m)
    subdivs=5,
    offset=0.015,       # shrinkwrap standoff
    thickness=0.010,    # solidify thickness
    bevel_width=0.0025,
    bevel_segments=2,
    use_project=False,
    project_dir="forward",  # "forward" = -Y front, "back" = +Y back
    project_limit=0.20,
    z_yaw_deg=0.0,          # (deprecated) yaw about Z — doesn't create visual V
    y_roll_deg=0.0,         # roll about Y — tilts plate so inner edge up, outer down
    pre_rotation=None,      # initial plane normal direction
):
    """Spawn a plate with the canonical modifier stack."""
    # Iter-2 FIX: scale BEFORE rotate. Previous order was rotation-first then
    # scale.z, but transform_apply bakes rotation (X+90°) which remaps axes:
    # the scale along "object Z" gets applied to what was originally object Y,
    # which is ALREADY Z=0 on the mesh verts → no scaling happens and the
    # plate stays 4cm × 4cm regardless of `height`. This silently collapsed
    # the sternum from 35.5cm tall to 4cm tall, producing the tiny sternum
    # stub seen in iter-2 renders.
    #
    # Correct order:
    #   1. Spawn plane flat (no rotation). Verts at (±w/2, ±w/2, 0).
    #   2. Apply XY scale in mesh-local space to stretch Y → desired height.
    #   3. Rotate the plane so its normal points -Y (or +Y for back plates).
    #   4. Apply rotation (scale already applied).
    # Default: plane-normal faces -Y (front-facing piece).
    if pre_rotation is None:
        pre_rotation = (math.radians(90), 0, 0)
    bpy.ops.mesh.primitive_plane_add(
        size=width,
        location=location,
        rotation=(0, 0, 0),   # spawn flat, then scale Y, then rotate
    )
    plate = bpy.context.active_object
    plate.name = name
    # Scale local Y by height/width so the flat plate is width x height in XY.
    plate.scale.y = height / width
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    # NOW apply the rotation to stand the plate up facing -Y (or +Y).
    plate.rotation_euler = pre_rotation
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)

    # Iter-2: chevron slant is a ROLL about Y (plate is in XZ plane facing -Y).
    # Rolling about Y tilts the plate so inner edge rises / outer drops →
    # visual chevron V. Z-yaw rotates plate in horizontal plane (not the
    # desired V effect) — kept as a deprecated escape hatch for now.
    if y_roll_deg != 0.0:
        plate.rotation_euler = (0, math.radians(y_roll_deg), 0)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    if z_yaw_deg != 0.0:
        plate.rotation_euler = (0, 0, math.radians(z_yaw_deg))
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)

    # Subdivide.
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.subdivide(number_cuts=subdivs)
    bpy.ops.object.mode_set(mode="OBJECT")

    # Shrinkwrap.
    sw = plate.modifiers.new("ArmorShrinkwrap", "SHRINKWRAP")
    sw.target = basemesh
    if use_project:
        sw.wrap_method = "PROJECT"
        sw.project_limit = project_limit
        sw.use_project_z = False
        sw.use_project_x = False
        sw.use_project_y = True
        if project_dir == "forward":
            # Plate is at front (-Y), rays shoot +Y to hit front of body.
            # Body front normals point in -Y; relative to +Y ray, they face
            # away → "BACK". cull_face="BACK" culls back-facing faces (normals
            # facing away from ray origin), so... wait, semantics are tricky.
            # Empirically from iter-1: cull_face="BACK" worked for front plates.
            sw.use_positive_direction = True
            sw.use_negative_direction = False
            sw.cull_face = "OFF"
        elif project_dir == "back":
            # Plate is at back (+Y), rays shoot -Y to hit back of body.
            # Iter-2 bug: cull_face="FRONT" culled all back-surface faces →
            # 0 hits → all verts stayed at spawn Y=+0.30 → trim_floaters
            # deleted every vert. Set cull_face="OFF" so the ray hits the
            # first surface regardless of normal orientation.
            sw.use_positive_direction = False
            sw.use_negative_direction = True
            sw.cull_face = "OFF"
    else:
        sw.wrap_method = "NEAREST_SURFACEPOINT"
    sw.offset = offset

    # Solidify.
    sol = plate.modifiers.new("ArmorSolidify", "SOLIDIFY")
    sol.thickness = thickness
    sol.offset = 1.0
    sol.use_even_offset = True
    sol.use_quality_normals = True

    # Bevel.
    bev = plate.modifiers.new("ArmorBevel", "BEVEL")
    bev.width = bevel_width
    bev.segments = bevel_segments
    bev.limit_method = "ANGLE"
    bev.angle_limit = 0.52

    # Weighted normal.
    wn = plate.modifiers.new("ArmorWeightedNormal", "WEIGHTED_NORMAL")
    wn.weight = 100

    bpy.ops.object.select_all(action="DESELECT")
    plate.select_set(True)
    bpy.context.view_layer.objects.active = plate
    bpy.ops.object.shade_smooth()
    return plate


def trim_floaters(plate, spawn_y, margin=0.03, absolute_cutoff=None):
    """Delete verts that never reached the body.

    Two-criterion trim:
      (1) Near-spawn trim: |Y - spawn_y| < margin  (caught verts where the
          PROJECT ray missed the body entirely).
      (2) Absolute cutoff: if Y is on the WRONG side of absolute_cutoff, delete.
          For front plates (spawn_y<0), cutoff is the max legit Y (e.g. -0.10);
          verts further forward than that (Y < -0.10) haven't reached the body.
          For back plates (spawn_y>0), cutoff is min legit Y (e.g. +0.17);
          verts further back than that (Y > +0.17) haven't reached the body.

    Iter-2 notes:
      - Previous threshold `abs(worldY) >= 0.23` wrongly trimmed valid verts.
      - Narrow near-spawn margin only catches exact-spawn verts; verts that
        stopped partway (at project_limit boundary) slip through. Use
        absolute_cutoff as the safety net.
    """
    bpy.context.view_layer.objects.active = plate
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(plate.data)
    bm.verts.ensure_lookup_table()
    mw = plate.matrix_world

    def is_floater(v):
        wy = (mw @ v.co).y
        if abs(wy - spawn_y) < margin:
            return True
        if absolute_cutoff is not None:
            if spawn_y < 0 and wy < absolute_cutoff:
                # Front plate: legit body Y at chest ≈ -0.22, cutoff -0.24 or so.
                return True
            if spawn_y > 0 and wy > absolute_cutoff:
                # Back plate: legit body Y at back ≈ +0.10, cutoff +0.17 or so.
                return True
        return False

    to_delete = [v for v in bm.verts if is_floater(v)]
    if to_delete:
        bmesh.ops.delete(bm, geom=to_delete, context="VERTS")
        bmesh.update_edit_mesh(plate.data)
    bpy.ops.object.mode_set(mode="OBJECT")


def make_chevron_half(
    name,
    z_center,
    x_center,           # center of this half (+ for right, - for left)
    width,              # half-chevron width (plate extent along local X)
    height,
    chevron_drop,       # how far bottom-center pushes down
    roll_deg,           # Y-axis roll: +N = inner edge UP, outer edge DOWN
    subdivs=5,
    offset=0.015,
    thickness=0.011,
    bevel_width=0.003,
    bevel_segments=2,
    use_project=True,
):
    """One half of a V-shape chevron — rectangular plate rolled about Y.

    Iter-2: roll_deg is rotation about the Y axis (not Z yaw). For a plate
    in the XZ plane facing -Y, this tilts the plate so one edge in X lifts
    and the other drops in Z — producing the visual V-sweep from sternum
    outward and downward.

    Pair convention: left_half (x_center<0) gets roll_deg = -18 so its
    inner (positive X) edge lifts; right_half (x_center>0) gets roll_deg
    = +18 so its inner (negative X) edge lifts. Mirror-symmetric.
    """
    plate = make_plate(
        name=name,
        location=(x_center, FRONT_Y, z_center),
        width=width,
        height=height,
        subdivs=subdivs,
        offset=offset,
        thickness=thickness,
        bevel_width=bevel_width,
        bevel_segments=bevel_segments,
        use_project=use_project,
        project_dir="forward",
        y_roll_deg=roll_deg,
    )

    # Edit-mode: push bottom-center verts DOWN by chevron_drop so even with
    # the yaw, the plate reads as a wedge that sweeps below its center.
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(plate.data)
    bm.verts.ensure_lookup_table()
    mw = plate.matrix_world
    mwi = mw.inverted()
    v_world = [mw @ v.co for v in bm.verts]
    lo_z = min(p.z for p in v_world); hi_z = max(p.z for p in v_world)
    for v, wp in zip(bm.verts, v_world):
        frac_z = (wp.z - lo_z) / ((hi_z - lo_z) + 1e-6)
        # Bottom 45% of plate: gentle drop so plate curves down toward hips.
        bottom_mask = max(0.0, 1.0 - frac_z * 2.2)
        drop_here = chevron_drop * bottom_mask
        new_world = Vector((wp.x, wp.y, wp.z - drop_here))
        v.co = mwi @ new_world
    bmesh.update_edit_mesh(plate.data)
    bpy.ops.object.mode_set(mode="OBJECT")
    return plate


def make_rib(name, z_center, width, height,
             offset=0.013, thickness=0.008, bevel_width=0.0018, bevel_segments=2):
    return make_plate(
        name=name,
        location=(0.0, FRONT_Y, z_center),
        width=width,
        height=height,
        subdivs=3,
        offset=offset,
        thickness=thickness,
        bevel_width=bevel_width,
        bevel_segments=bevel_segments,
        use_project=False,  # narrow ribs (<= 28cm) fit on chest w/o wrap
    )


def make_back_plate(name, location, width, height,
                    subdivs=3, offset=0.013, thickness=0.009,
                    bevel_width=0.002, bevel_segments=2,
                    use_project=True):
    """Back-facing plate — plane-normal points +Y, project toward -Y to hit back surface.

    Iter-2: project_limit bumped to 0.30 (default 0.20 couldn't reach upper
    spine from BACK_Y=+0.22: distance from +0.22 to spine Y≈+0.05 = 17cm,
    margin=3cm). With BACK_Y=+0.22 and project_limit=0.30 we have plenty of
    reach across the whole back Z range.
    """
    return make_plate(
        name=name,
        location=location,
        width=width,
        height=height,
        subdivs=subdivs,
        offset=offset,
        thickness=thickness,
        bevel_width=bevel_width,
        bevel_segments=bevel_segments,
        use_project=use_project,
        project_dir="back",
        project_limit=0.30,
        pre_rotation=(math.radians(-90), 0, 0),  # plane normal -> +Y (rear)
    )


# --- 2. SternUM RIDGE — placed FIRST, centered X=0, runs FULL chest height ---
# From just above collarbone notch (Z_COLLAR) down to navel (Z_NAVEL).
# Spec: 4cm wide, ~60cm tall.
STERNUM_Z_CENTER = (Z_COLLAR + Z_NAVEL) * 0.5
STERNUM_HEIGHT   = (Z_COLLAR - Z_NAVEL) + 0.02

sternum = make_plate(
    name="torso_sternum",
    location=(0.0, FRONT_Y, STERNUM_Z_CENTER),
    width=0.04,
    height=STERNUM_HEIGHT,
    subdivs=6,
    offset=0.032,        # tallest proud — sits on top of chevrons
    thickness=0.014,
    bevel_width=0.002,
    bevel_segments=2,
    use_project=True,
)
# Trim any floater verts at spawn depth (tall narrow ridge mostly hits body).
# Sternum is narrow, unlikely to float — keep trim for safety.

# --- 3. Chevron chest plates — 3 rows × 2 halves = 6 pieces ---
# Row widths (per spec, total chevron width, half = width/2):
# top: 38cm (half=19), mid: 34cm (half=17), low: 30cm (half=15)
# Each half is yawed ±18° about Z so inner edges meet at sternum, outer edges
# sweep down-and-out.
CHEVRON_ROLL = 18.0   # degrees Y-axis roll

chevron_01_l = make_chevron_half(
    name="torso_chevron_01_top_l",
    z_center=Z_UPPER_CHEST,
    x_center=-0.095,        # inner edge touches sternum (local +X side)
    width=0.19,             # half of 38cm
    height=0.075,
    chevron_drop=0.050,
    roll_deg=+CHEVRON_ROLL, # local +X (inner edge) rises
    offset=0.012,
    thickness=0.011,
    use_project=True,
)
chevron_01_r = make_chevron_half(
    name="torso_chevron_01_top_r",
    z_center=Z_UPPER_CHEST,
    x_center=+0.095,
    width=0.19,
    height=0.075,
    chevron_drop=0.050,
    roll_deg=-CHEVRON_ROLL, # mirror: local -X (inner edge) rises
    offset=0.012,
    thickness=0.011,
    use_project=True,
)

chevron_02_l = make_chevron_half(
    name="torso_chevron_02_mid_l",
    z_center=Z_MID_CHEST,
    x_center=-0.085,
    width=0.17,             # half of 34cm
    height=0.070,
    chevron_drop=0.044,
    roll_deg=+CHEVRON_ROLL,
    offset=0.022,
    thickness=0.011,
    use_project=True,
)
chevron_02_r = make_chevron_half(
    name="torso_chevron_02_mid_r",
    z_center=Z_MID_CHEST,
    x_center=+0.085,
    width=0.17,
    height=0.070,
    chevron_drop=0.044,
    roll_deg=-CHEVRON_ROLL,
    offset=0.022,
    thickness=0.011,
    use_project=True,
)

chevron_03_l = make_chevron_half(
    name="torso_chevron_03_low_l",
    z_center=Z_LOW_CHEST,
    x_center=-0.075,
    width=0.15,             # half of 30cm
    height=0.060,
    chevron_drop=0.036,
    roll_deg=+CHEVRON_ROLL,
    offset=0.018,
    thickness=0.010,
    use_project=True,
)
chevron_03_r = make_chevron_half(
    name="torso_chevron_03_low_r",
    z_center=Z_LOW_CHEST,
    x_center=+0.075,
    width=0.15,
    height=0.060,
    chevron_drop=0.036,
    roll_deg=-CHEVRON_ROLL,
    offset=0.018,
    thickness=0.010,
    use_project=True,
)

# --- 4. Ribbed abdominal segments — 5 horizontal ribs, widened 28->20cm ---
RIB_TOP_Z     = Z_DIAPHRAGM - 0.015
RIB_BOTTOM_Z  = Z_NAVEL + 0.005
RIB_COUNT     = 5
rib_spacing   = (RIB_TOP_Z - RIB_BOTTOM_Z) / (RIB_COUNT - 1)
RIB_WIDTHS    = [0.28, 0.26, 0.24, 0.22, 0.20]  # widened per spec
RIB_HEIGHT    = 0.030

ribs = []
for i in range(RIB_COUNT):
    z = RIB_TOP_Z - i * rib_spacing
    w = RIB_WIDTHS[i]
    rib = make_rib(
        name=f"torso_rib_{i+1:02d}",
        z_center=z,
        width=w,
        height=RIB_HEIGHT,
        offset=0.013 + (i % 2) * 0.004,
        thickness=0.008,
    )
    ribs.append(rib)

# --- 5. BACK PLATES — defect #2 fix ---

# 5a. Spine vertebra boxes — 7 small boxes down the back centerline.
# Each is a small plate (6cm wide x 4cm tall) placed at +Y (behind body).
vertebra_count = 7
vertebra_z_top = Z_SPINE_TOP
vertebra_z_bot = Z_SPINE_BOT
vertebra_spacing = (vertebra_z_top - vertebra_z_bot) / (vertebra_count - 1)

vertebrae = []
for i in range(vertebra_count):
    z = vertebra_z_top - i * vertebra_spacing
    # Vertebrae taper slightly narrower toward top (like real spine segments).
    taper = 1.0 - 0.10 * (i / max(1, vertebra_count - 1))
    vb = make_back_plate(
        name=f"torso_vertebra_{i+1:02d}",
        location=(0.0, BACK_Y, z),
        width=0.065 * taper,
        height=0.040,
        offset=0.025 + (i % 2) * 0.005,   # alternating standoff for ridge effect
        thickness=0.012,
        bevel_width=0.0022,
    )
    vertebrae.append(vb)

# 5b. Scapula plates — 2 trapezoid plates flanking upper spine.
# Place them at shoulder-blade height (Z 0.67-0.735 band), offset +/-0.11 in X.
def make_scapula(name, x_center, z_center, width, height):
    plate = make_back_plate(
        name=name,
        location=(x_center, BACK_Y, z_center),
        width=width,
        height=height,
        subdivs=4,
        offset=0.018,
        thickness=0.010,
        bevel_width=0.0025,
    )
    # Trapezoid-ify: pinch the bottom edge inward so the plate looks like a
    # shoulder-blade (wider at top, narrower at bottom).
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(plate.data)
    bm.verts.ensure_lookup_table()
    mw = plate.matrix_world
    mwi = mw.inverted()
    v_world = [mw @ v.co for v in bm.verts]
    lo_z = min(p.z for p in v_world); hi_z = max(p.z for p in v_world)
    for v, wp in zip(bm.verts, v_world):
        frac_z = (wp.z - lo_z) / ((hi_z - lo_z) + 1e-6)
        # Pull bottom verts inward toward x_center by up to 25% of half-width.
        bottom_pinch = max(0.0, 1.0 - frac_z) * 0.25
        dx = (x_center - wp.x) * bottom_pinch
        new_world = Vector((wp.x + dx, wp.y, wp.z))
        v.co = mwi @ new_world
    bmesh.update_edit_mesh(plate.data)
    bpy.ops.object.mode_set(mode="OBJECT")
    return plate

SCAPULA_Z_CENTER = (Z_SHOULDER_BLADE_HI + Z_SHOULDER_BLADE_LO) * 0.5
SCAPULA_HEIGHT   = Z_SHOULDER_BLADE_HI - Z_SHOULDER_BLADE_LO + 0.015

scapula_l = make_scapula(
    name="torso_scapula_l",
    x_center=-0.13,
    z_center=SCAPULA_Z_CENTER,
    width=0.17,
    height=SCAPULA_HEIGHT,
)
scapula_r = make_scapula(
    name="torso_scapula_r",
    x_center=+0.13,
    z_center=SCAPULA_Z_CENTER,
    width=0.17,
    height=SCAPULA_HEIGHT,
)

# 5c. Lumbar band — single ribbed plate low-back.
lumbar = make_back_plate(
    name="torso_lumbar",
    location=(0.0, BACK_Y, Z_LUMBAR),
    width=0.28,
    height=0.048,
    subdivs=3,
    offset=0.013,
    thickness=0.010,
    bevel_width=0.0022,
)

# --- 6. Etherealism matte material for armor ---
armor_mat = bpy.data.materials.new(name="torso_armor_eth")
armor_mat.use_nodes = True
nodes = armor_mat.node_tree.nodes
for n in list(nodes): nodes.remove(n)
out = nodes.new("ShaderNodeOutputMaterial")
bsdf = nodes.new("ShaderNodeBsdfPrincipled")
armor_mat.node_tree.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
# Iter-2: brighten armor to establish the Etherealism value contrast against
# the 0.08 undersuit. Previous 0.10 albedo was indistinguishable from body
# under Filmic tone-mapping + rim lights. 0.22 albedo + 0.35 metallic makes
# armor read as a distinctly brighter, metallic piece against the dark suit.
bsdf.inputs["Base Color"].default_value = (0.22, 0.22, 0.24, 1.0)
bsdf.inputs["Metallic"].default_value = 0.35
bsdf.inputs["Roughness"].default_value = 0.55

armor_pieces = (
    [sternum, chevron_01_l, chevron_01_r, chevron_02_l, chevron_02_r,
     chevron_03_l, chevron_03_r] +
    ribs +
    vertebrae +
    [scapula_l, scapula_r, lumbar]
)
for piece in armor_pieces:
    piece.data.materials.clear()
    piece.data.materials.append(armor_mat)

# --- 7. Dark undersuit material for base body (per defect #3 fix) ---
# Spec: albedo (0.08, 0.08, 0.10), roughness 0.85, metallic 0.0.
# Also: drop the Principled "Specular IOR Level" (or legacy "Specular") to
# a low value so the hot rim lights don't wash the suit out to near-white.
skin_mat = bpy.data.materials.new(name="body_undersuit_eth")
skin_mat.use_nodes = True
snodes = skin_mat.node_tree.nodes
for n in list(snodes): snodes.remove(n)
s_out = snodes.new("ShaderNodeOutputMaterial")
s_bsdf = snodes.new("ShaderNodeBsdfPrincipled")
skin_mat.node_tree.links.new(s_bsdf.outputs["BSDF"], s_out.inputs["Surface"])
# Iter-2 attempt 4: spec says (0.08, 0.08, 0.10) but under Filmic + hot rim
# lights, 0.08 still renders ~40% gray. Target: body should read MUCH darker
# than armor (armor albedo 0.22). Drop to 0.030 — barely lit silhouette shape
# that reads as "dark bodysuit" against the lit armor plates.
s_bsdf.inputs["Base Color"].default_value = (0.030, 0.030, 0.045, 1.0)
s_bsdf.inputs["Metallic"].default_value = 0.0
s_bsdf.inputs["Roughness"].default_value = 0.90
# Suppress specular highlight so rim light doesn't overexpose the suit.
# Blender 4.2 renamed "Specular" → "Specular IOR Level" (0..1 factor).
for key in ("Specular IOR Level", "Specular"):
    if key in s_bsdf.inputs:
        s_bsdf.inputs[key].default_value = 0.10
        break
basemesh.data.materials.clear()
basemesh.data.materials.append(skin_mat)

# --- 8. Apply modifier stacks, trim floaters ---
# Build the spawn_y map so each piece gets the correct trim reference.
front_pieces = {sternum,
                chevron_01_l, chevron_01_r,
                chevron_02_l, chevron_02_r,
                chevron_03_l, chevron_03_r} | set(ribs)
back_pieces  = set(vertebrae) | {scapula_l, scapula_r, lumbar}

bpy.ops.object.select_all(action="DESELECT")
for piece in armor_pieces:
    bpy.context.view_layer.objects.active = piece
    for m in list(piece.modifiers):
        try:
            bpy.ops.object.modifier_apply(modifier=m.name)
        except RuntimeError as e:
            print(f"  warn: could not apply {m.name} on {piece.name}: {e}")
    if piece in front_pieces:
        # Body chest/ab front Y ≈ -0.22; legit armor verts at Y ≈ -0.19 to -0.22.
        # Cutoff: verts more negative than -0.25 are floaters stuck forward.
        trim_floaters(piece, spawn_y=FRONT_Y, margin=0.02, absolute_cutoff=-0.25)
    elif piece in back_pieces:
        # Body back Y max ≈ +0.12; legit verts at Y ≈ +0.10 to +0.14.
        # Cutoff: verts more positive than +0.17 are floaters stuck rearward.
        trim_floaters(piece, spawn_y=BACK_Y, margin=0.02, absolute_cutoff=+0.17)

bpy.ops.object.select_all(action="DESELECT")

# --- 9. Save authoring .blend ---
bpy.ops.wm.save_as_mainfile(filepath=DST_BLEND)
print(f"[torso_armor] authoring .blend saved -> {DST_BLEND}")

# --- 10. Build joined export mesh ---
export_pieces = []
for piece in armor_pieces:
    bpy.context.view_layer.objects.active = piece
    piece.select_set(True)
    bpy.ops.object.duplicate()
    dup = bpy.context.active_object
    dup.name = piece.name + "_exp"
    export_pieces.append(dup)
    bpy.ops.object.select_all(action="DESELECT")
    piece.select_set(False)

for dup in export_pieces:
    dup.select_set(True)
bpy.context.view_layer.objects.active = export_pieces[0]
bpy.ops.object.join()
joined = bpy.context.active_object
joined.name = "torso_armor_export"

me = joined.data
me.calc_loop_triangles()
tri_count = len(me.loop_triangles)
vert_count = len(me.vertices)
print(f"[torso_armor] joined export mesh: verts={vert_count}  tris={tri_count}")

# --- 11. Export OBJ ---
bpy.ops.object.select_all(action="DESELECT")
joined.select_set(True)
bpy.context.view_layer.objects.active = joined

if hasattr(bpy.ops.wm, "obj_export"):
    bpy.ops.wm.obj_export(
        filepath=DST_OBJ,
        export_selected_objects=True,
        apply_modifiers=True,
        export_normals=True,
        export_uv=False,
        export_materials=False,
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
    )
else:
    bpy.ops.export_scene.obj(
        filepath=DST_OBJ,
        use_selection=True,
        use_normals=True,
        use_materials=False,
    )
print(f"[torso_armor] OBJ exported -> {DST_OBJ}")

# --- 12. Hide export mesh, resave ---
joined.hide_render = True
joined.hide_viewport = True
bpy.ops.wm.save_as_mainfile(filepath=DST_BLEND)
print(f"[torso_armor] final .blend saved -> {DST_BLEND}")
print(f"[torso_armor] pieces ({len(armor_pieces)}):")
for p in armor_pieces:
    print(f"    - {p.name}")
print(f"[torso_armor] tri_count={tri_count}  vert_count={vert_count}")

# --- 13. Per-piece AABB readout ---
bpy.context.view_layer.update()
print("[torso_armor] per-piece world AABB (modifiers applied):")
for piece in armor_pieces:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    piece_eval = piece.evaluated_get(depsgraph)
    ev_mesh = piece_eval.to_mesh()
    mw = piece.matrix_world
    verts = [mw @ v.co for v in ev_mesh.vertices]
    if verts:
        zs = [v.z for v in verts]; xs = [v.x for v in verts]; ys = [v.y for v in verts]
        print(f"  {piece.name:30s} Z=[{min(zs):+.3f},{max(zs):+.3f}] "
              f"Y=[{min(ys):+.3f},{max(ys):+.3f}] X=[{min(xs):+.3f},{max(xs):+.3f}]")
    piece_eval.to_mesh_clear()

print("--- DONE (iter-2) ---")
