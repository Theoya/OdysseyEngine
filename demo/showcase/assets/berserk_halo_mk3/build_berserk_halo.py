"""Berserk-Halo Mk3 Mjolnir — piece-by-piece Blender 2.93 build script.

Run headless:
    "C:/Program Files/Blender Foundation/Blender 2.93/blender.exe" --background --python build_berserk_halo.py

Produces (in the script's directory):
    berserk_halo_master.blend    — authoring file, all 35+ named components
    berserk_halo.obj             — triangulated game export, +Y up, -Z forward
    berserk_halo.mtl             — monochrome material stub (engine uses .mat.xml instead)
    tri_counts.txt               — per-component final tri counts after modifiers apply

Design:
    - 1 Blender unit == 1 m. Pivot feet-center at origin (0,0,0).
    - Humanoid scale: 1.80 m tall, armor adds ~25% bulk.
    - Hard facts used for bone positions (world-space rest pose, derived from
      demo/fps_humanoid/assets/humanoid.skeleton.xml by summing parent offsets):
          head     y=1.55   (root.y + spine.y + chest.y + neck.y + head.y/2 offset)
          chest    y=1.35
          spine    y=1.15
          root     y=0.95   (pelvis center)
          upper_arm.L/R y=1.07, x=±0.15 (shoulder down -0.28/2)
          lower_arm.L/R y=0.79, x=±0.15
          hand.L/R      y=0.62, x=±0.15
          upper_leg.L/R y=0.35, x=±0.10 (root -0.4/2 + 0.4)
          lower_leg.L/R y=0.05, x=±0.10
          foot.L/R      y=-0.075, x=±0.10  — but we clamp armor above ground

    Style: brutal / angular / heavy / scarred. Layered plates, forward hunch (±3deg),
    devil-horn forward-swept helmet, dragon-scale pauldron stack, ribbed abdomen.
"""
import bpy
import bmesh
import os
from mathutils import Vector
from math import radians

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
HEIGHT    = 1.80
BULK      = 1.28                 # slightly over the "1.25-1.30x" spec for brutal read
HUNCH_DEG = 3.0                  # forward-lean of the torso
FWD       = 0.02                 # small forward offset so chest/abdomen read 3D

OUT_BLEND  = "berserk_halo_master.blend"
OUT_OBJ    = "berserk_halo.obj"
OUT_COUNTS = "tri_counts.txt"

_created = []   # (name, obj) log in build order

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def reset_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    # Also purge orphaned meshes so repeat runs stay clean.
    for block in list(bpy.data.meshes):
        if block.users == 0:
            bpy.data.meshes.remove(block)

def _register(name, obj):
    obj.name = name
    _created.append((name, obj))
    return obj

def add_box(name, size, location, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location, rotation=rotation)
    obj = bpy.context.active_object
    obj.scale = Vector(size)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return _register(name, obj)

def add_cyl(name, radius, depth, location, rotation=(0, 0, 0), vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(
        radius=radius, depth=depth, location=location, rotation=rotation,
        vertices=vertices,
    )
    return _register(name, bpy.context.active_object)

def add_cone(name, r1, r2, depth, location, rotation=(0, 0, 0), vertices=10):
    bpy.ops.mesh.primitive_cone_add(
        radius1=r1, radius2=r2, depth=depth, location=location, rotation=rotation,
        vertices=vertices,
    )
    return _register(name, bpy.context.active_object)

def add_uvsphere(name, radius, location, segments=16, rings=10):
    bpy.ops.mesh.primitive_uv_sphere_add(
        radius=radius, location=location, segments=segments, ring_count=rings,
    )
    return _register(name, bpy.context.active_object)

def solidify_bevel(obj, thickness=0.018, bevel_width=0.008, bevel_segments=2):
    """Standard plate treatment: shell, crisp bevel, weighted normals."""
    bpy.context.view_layer.objects.active = obj
    m = obj.modifiers.new("Solidify", "SOLIDIFY")
    m.thickness = thickness
    m.offset = 1.0
    m = obj.modifiers.new("Bevel", "BEVEL")
    m.width = bevel_width
    m.segments = bevel_segments
    m.limit_method = "ANGLE"
    m.angle_limit = radians(35)
    m = obj.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
    m.keep_sharp = True

def bevel_only(obj, bevel_width=0.005, bevel_segments=2):
    bpy.context.view_layer.objects.active = obj
    m = obj.modifiers.new("Bevel", "BEVEL")
    m.width = bevel_width
    m.segments = bevel_segments
    m.limit_method = "ANGLE"
    m.angle_limit = radians(35)

def scar_notch(obj, location_local, size=0.015):
    """Add a small subtractive notch by editing mesh in-place (cheap scar)."""
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(obj.data)
    # Find a face closest to requested local point and inset+move inward.
    target = Vector(location_local)
    best = None
    best_dist = 1e9
    for f in bm.faces:
        d = (f.calc_center_median() - target).length
        if d < best_dist:
            best_dist = d
            best = f
    if best is not None:
        bmesh.ops.inset_individual(bm, faces=[best], thickness=size, depth=-size*0.5)
    bmesh.update_edit_mesh(obj.data)
    bpy.ops.object.mode_set(mode="OBJECT")

# ---------------------------------------------------------------------------
# Component builders — piece by piece
# ---------------------------------------------------------------------------
def build_helmet():
    # 1. Cranium shell — angular predatory skull (slightly faceted box).
    shell = add_box("helmet_shell",
                    (0.24*BULK, 0.30, 0.30*BULK),
                    (0, 1.60, 0.02))
    # Taper front to suggest a snout/predator cheek.
    bpy.context.view_layer.objects.active = shell
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(shell.data)
    for v in bm.verts:
        if v.co.z > 0.1:
            v.co.x *= 0.75
            v.co.y -= 0.04
    bmesh.update_edit_mesh(shell.data)
    bpy.ops.object.mode_set(mode="OBJECT")
    bevel_only(shell, bevel_width=0.010, bevel_segments=2)

    # 2 & 3. Horns — forward-swept cones.
    for side, xs in (("l", -1), ("r", 1)):
        horn = add_cone(f"horn_{side}",
                        r1=0.035, r2=0.003, depth=0.30,
                        location=(xs*0.11, 1.78, 0.08),
                        rotation=(radians(-55), 0, radians(xs*12)),
                        vertices=10)
        bevel_only(horn, bevel_width=0.003, bevel_segments=1)

    # 4. Visor slit — thin emissive strip. Named distinct for material assignment.
    visor = add_box("visor_slit",
                    (0.22, 0.020, 0.008),
                    (0, 1.60, 0.18*BULK))
    # Mark material slot name so .mtl export names it.
    visor_mat = bpy.data.materials.new("berserk_halo_visor")
    visor_mat.diffuse_color = (1.0, 1.0, 1.0, 1.0)
    visor.data.materials.append(visor_mat)

    # 5 & 6. Cheek plates — swept faceted slabs under visor.
    for side, xs in (("l", -1), ("r", 1)):
        cheek = add_box(f"cheek_{side}",
                        (0.09, 0.10, 0.14),
                        (xs*0.11, 1.52, 0.11),
                        rotation=(0, radians(xs*-15), 0))
        solidify_bevel(cheek, thickness=0.012, bevel_width=0.006)

def build_torso():
    hunch = radians(HUNCH_DEG)

    # 7. Sternum ridge — central vertical spine on the chest.
    sternum = add_box("sternum_ridge",
                      (0.04, 0.32, 0.06),
                      (0, 1.28, 0.14*BULK),
                      rotation=(hunch, 0, 0))
    bevel_only(sternum, bevel_width=0.004, bevel_segments=2)

    # 8 & 9. Chest carapace upper L/R — chevron plates radiating from sternum.
    for side, xs in (("l", -1), ("r", 1)):
        plate = add_box(f"chest_upper_{side}",
                        (0.22*BULK, 0.18, 0.14*BULK),
                        (xs*0.14, 1.38, 0.08),
                        rotation=(hunch, 0, radians(xs*-18)))
        solidify_bevel(plate, thickness=0.022, bevel_width=0.010)
        scar_notch(plate, (0.02, 0, 0.05), size=0.012)   # tiny hit-mark scar

    # 10 & 11. Chest carapace lower L/R — second chevron row.
    for side, xs in (("l", -1), ("r", 1)):
        plate = add_box(f"chest_lower_{side}",
                        (0.20*BULK, 0.14, 0.14*BULK),
                        (xs*0.13, 1.22, 0.07),
                        rotation=(hunch, 0, radians(xs*-12)))
        solidify_bevel(plate, thickness=0.020, bevel_width=0.009)

    # 12-14. Rib segments — horizontal tapering slabs down the abdomen.
    rib_ys = [1.08, 1.00, 0.93]
    rib_ws = [0.34, 0.32, 0.30]
    for i, (y, w) in enumerate(zip(rib_ys, rib_ws)):
        rib = add_box(f"rib_{i+1}",
                      (w*BULK, 0.055, 0.20*BULK),
                      (0, y, 0.04 + FWD),
                      rotation=(hunch, 0, 0))
        solidify_bevel(rib, thickness=0.014, bevel_width=0.006)

def build_back():
    # 23. Spine column — 7 stacked vertebra plates (small boxes).
    for i in range(7):
        y = 1.50 - i * 0.09
        seg = add_box(f"spine_vert_{i}",
                      (0.08, 0.06, 0.08),
                      (0, y, -0.14))
        bevel_only(seg, bevel_width=0.004, bevel_segments=2)

    # 24. Scapula plates L/R — trapezoids on upper back.
    for side, xs in (("l", -1), ("r", 1)):
        scap = add_box(f"scapula_{side}",
                       (0.22, 0.24, 0.06),
                       (xs*0.18, 1.42, -0.12),
                       rotation=(radians(-8), 0, radians(xs*-6)))
        solidify_bevel(scap, thickness=0.016, bevel_width=0.007)

    # 25. Upper-back hump — raised dome (jetpack housing read).
    hump = add_uvsphere("back_hump", radius=0.18,
                        location=(0, 1.48, -0.20),
                        segments=18, rings=10)
    hump.scale = Vector((1.0, 0.85, 0.7))
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bevel_only(hump, bevel_width=0.005, bevel_segments=1)

def build_shoulders_and_arms():
    hunch = radians(HUNCH_DEG)
    for side, xs in (("l", -1), ("r", 1)):
        x_shoulder = xs * 0.26
        # 15-17. Pauldron stack — 3 overlapping down-swept plates.
        for i in range(3):
            w = 0.22 - i * 0.03
            h = 0.10 - i * 0.015
            y = 1.45 - i * 0.07
            plate = add_box(f"pauldron_{side}_{i+1}",
                            (w*BULK, h, 0.22*BULK),
                            (x_shoulder, y, 0.02),
                            rotation=(hunch, 0, radians(xs*-20)))
            solidify_bevel(plate, thickness=0.016, bevel_width=0.008)
            if i == 2:
                scar_notch(plate, (w*0.4, 0, 0.05), size=0.012)

        # 18. Upper arm cylinder.
        upper = add_cyl(f"upper_arm_{side}",
                        radius=0.075, depth=0.26,
                        location=(xs*0.28, 1.20, 0),
                        vertices=16)
        bevel_only(upper, bevel_width=0.004, bevel_segments=2)

        # 19. Elbow lobster — 3 overlapping disc plates.
        for i in range(3):
            ring = add_cyl(f"elbow_{side}_{i+1}",
                           radius=0.085 - i*0.005, depth=0.035,
                           location=(xs*0.28, 1.04 - i*0.025, 0),
                           vertices=14)
            bevel_only(ring, bevel_width=0.002, bevel_segments=1)

        # 20. Forearm — tapered cylinder (approximated with a cylinder).
        fore = add_cyl(f"forearm_{side}",
                       radius=0.072, depth=0.25,
                       location=(xs*0.28, 0.85, 0),
                       vertices=16)
        bevel_only(fore, bevel_width=0.004, bevel_segments=2)

        # 21. Gauntlet — boxy with flared knuckles.
        gaunt = add_box(f"gauntlet_{side}",
                        (0.13*BULK, 0.14, 0.12*BULK),
                        (xs*0.28, 0.68, 0.03))
        solidify_bevel(gaunt, thickness=0.012, bevel_width=0.006)

        # 22. Claws — five small cones merging into one mesh.
        # Build 5 cones, then join into a single mesh object.
        claw_objs = []
        for j in range(5):
            offset = (j - 2) * 0.024
            claw = add_cone(f"_claw_{side}_{j}",
                            r1=0.011, r2=0.002, depth=0.06,
                            location=(xs*0.28 + offset, 0.60, 0.10),
                            rotation=(radians(90), 0, 0),
                            vertices=8)
            claw_objs.append(claw)
        # Join into a single 'claws_{side}' object.
        bpy.ops.object.select_all(action="DESELECT")
        for c in claw_objs:
            c.select_set(True)
        bpy.context.view_layer.objects.active = claw_objs[0]
        bpy.ops.object.join()
        joined = bpy.context.active_object
        joined.name = f"claws_{side}"
        # Remove stale entries, add the joined result.
        _created[:] = [(n, o) for (n, o) in _created if not n.startswith(f"_claw_{side}_")]
        _created.append((joined.name, joined))

def build_hips_and_legs():
    # 26. Hip belt — segmented ring (approx: box + 4 panel insets).
    belt = add_box("hip_belt",
                   (0.42*BULK, 0.14, 0.32*BULK),
                   (0, 0.86, 0))
    solidify_bevel(belt, thickness=0.020, bevel_width=0.010)

    # 27. Codpiece — downward triangular plate.
    cod = add_cone("codpiece",
                   r1=0.14, r2=0.04, depth=0.18,
                   location=(0, 0.74, 0.12),
                   rotation=(radians(180), 0, radians(45)),
                   vertices=4)
    bevel_only(cod, bevel_width=0.006, bevel_segments=1)

    for side, xs in (("l", -1), ("r", 1)):
        # 28. Upper leg plate — split front/rear overlapping plates.
        thigh = add_cyl(f"upper_leg_{side}",
                        radius=0.10, depth=0.40,
                        location=(xs*0.12, 0.58, 0),
                        vertices=14)
        bevel_only(thigh, bevel_width=0.004, bevel_segments=2)
        # Overlapping front plate for layered read.
        front = add_box(f"upper_leg_plate_front_{side}",
                        (0.14, 0.26, 0.04),
                        (xs*0.12, 0.58, 0.10))
        solidify_bevel(front, thickness=0.012, bevel_width=0.006)

        # 29. Knee cap + lobster — 3 stacked disks.
        for i in range(3):
            disk = add_cyl(f"knee_{side}_{i+1}",
                           radius=0.095 - i*0.006, depth=0.035,
                           location=(xs*0.12, 0.40 - i*0.025, 0.02),
                           vertices=14)
            bevel_only(disk, bevel_width=0.002, bevel_segments=1)

        # 30. Lower leg plate (greave).
        shin = add_cyl(f"lower_leg_{side}",
                       radius=0.095, depth=0.40,
                       location=(xs*0.12, 0.18, 0),
                       vertices=14)
        bevel_only(shin, bevel_width=0.004, bevel_segments=2)
        # Front greave overlay for layered read.
        greave = add_box(f"greave_{side}",
                         (0.13, 0.28, 0.04),
                         (xs*0.12, 0.18, 0.09))
        solidify_bevel(greave, thickness=0.012, bevel_width=0.006)

        # 31. Boot — heavy wedged/hooved shape.
        boot = add_box(f"boot_{side}",
                       (0.17*BULK, 0.13, 0.30*BULK),
                       (xs*0.12, 0.04, 0.06))
        solidify_bevel(boot, thickness=0.016, bevel_width=0.008)
        # Toe cap wedge at front.
        toe = add_box(f"boot_toe_{side}",
                      (0.15, 0.07, 0.08),
                      (xs*0.12, 0.08, 0.20))
        bevel_only(toe, bevel_width=0.006, bevel_segments=2)

# ---------------------------------------------------------------------------
# Assembly, tri-count, export
# ---------------------------------------------------------------------------
def count_tris_after_modifiers(obj):
    """Evaluate the mesh with all modifiers and return its triangle count."""
    dg = bpy.context.evaluated_depsgraph_get()
    eval_obj = obj.evaluated_get(dg)
    me = eval_obj.to_mesh()
    n = 0
    for p in me.polygons:
        n += max(1, len(p.vertices) - 2)    # fan-triangulation count
    eval_obj.to_mesh_clear()
    return n

def parent_to_root():
    bpy.ops.object.empty_add(type="ARROWS", location=(0, 0, 0))
    root = bpy.context.active_object
    root.name = "berserk_halo_root"
    for name, obj in _created:
        if obj.type == "MESH":
            obj.parent = root
    return root

def write_tri_counts(filepath):
    total = 0
    lines = []
    for name, obj in _created:
        if obj.type != "MESH":
            continue
        tris = count_tris_after_modifiers(obj)
        total += tris
        lines.append(f"{tris:6d}  {name}")
    lines.append(f"------")
    lines.append(f"{total:6d}  TOTAL")
    with open(filepath, "w") as f:
        f.write("\n".join(lines))
    print("\n".join(lines))
    return total

def apply_all_modifiers():
    for name, obj in list(_created):
        if obj.type != "MESH":
            continue
        bpy.context.view_layer.objects.active = obj
        # Apply modifiers in order.
        for mod in list(obj.modifiers):
            try:
                bpy.ops.object.modifier_apply(modifier=mod.name)
            except RuntimeError as exc:
                print(f"[warn] could not apply {mod.name} on {name}: {exc}")

def main():
    here = os.path.dirname(bpy.data.filepath) if bpy.data.filepath else ""
    # Resolve output dir relative to this script when run via --python.
    script_path = os.path.abspath(__file__) if "__file__" in globals() else ""
    out_dir = os.path.dirname(script_path) if script_path else here
    if not out_dir:
        out_dir = os.getcwd()

    reset_scene()

    build_helmet()
    build_torso()
    build_back()
    build_shoulders_and_arms()
    build_hips_and_legs()

    root = parent_to_root()

    # Tri counts before modifier apply (cheap; modifiers are still applied at export).
    counts_path = os.path.join(out_dir, OUT_COUNTS)
    total = write_tri_counts(counts_path)
    print(f"[build] component tri total (post-modifier eval): {total}")

    # Save .blend BEFORE applying modifiers so the authoring file remains editable.
    blend_path = os.path.join(out_dir, OUT_BLEND)
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    print(f"[build] authoring .blend saved: {blend_path}")

    # For the OBJ export, we can rely on apply_modifiers=True flag instead of applying
    # manually — preserves the .blend file's non-destructive stack.
    obj_path = os.path.join(out_dir, OUT_OBJ)
    # Select all meshes for export.
    bpy.ops.object.select_all(action="DESELECT")
    for name, obj in _created:
        if obj.type == "MESH":
            obj.select_set(True)
    bpy.ops.export_scene.obj(
        filepath=obj_path,
        use_selection=True,
        use_triangles=True,
        use_normals=True,
        use_uvs=False,
        use_materials=True,
        axis_forward="-Z",
        axis_up="Y",
        use_mesh_modifiers=True,
    )
    print(f"[build] OBJ exported: {obj_path}")

if __name__ == "__main__":
    main()
