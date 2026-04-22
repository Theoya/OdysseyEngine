"""
Phase 3 — Cloth simulation & bake over the Walking animation.

Starts from `cloak.blend` produced in Phase 2.

Does:
  - adds Cloth modifier to cloak with heavier/slower settings for cinematic drape
  - sets Pin Group = 'pin_ring'
  - adds Collision modifier to the character body mesh
  - adds a FORCE FIELD (Wind) at constant ~0.6 m/s drifting roughly backward-up
    so the cloak never reads limp; this is the thematic-wind trick
  - adds a light TURBULENCE force field for organic variation
  - extends the scene frame range with 30 pre-roll frames so the cloth
    settles before the walk starts
  - bakes the simulation for frame_start..frame_end
  - saves to cloaked_walking.blend

Run:
  "C:\\Users\\THadfield\\Blender 4.2\\blender-4.2.20-windows-x64\\blender.exe" \
      --background --python phase3_cloth_sim.py
"""

import bpy
import os
import math

PROJECT_ROOT = r"T:/OdysseyEngine"
SRC_BLEND = os.path.join(PROJECT_ROOT, "third_party", "obsidian_sentinel_cloaked", "cloak.blend")
OUT_BLEND = os.path.join(PROJECT_ROOT, "third_party", "obsidian_sentinel_cloaked", "cloaked_walking.blend")

bpy.ops.wm.open_mainfile(filepath=SRC_BLEND)

print("=" * 70); print("PHASE 3: Cloth simulation setup"); print("=" * 70)

cloak = bpy.data.objects.get("cloak")
# Body mesh is the non-cloak mesh
body = next((o for o in bpy.data.objects if o.type == "MESH" and o.name != "cloak"), None)
arm = next((o for o in bpy.data.objects if o.type == "ARMATURE"), None)
assert cloak and body and arm, "Scene missing cloak/body/armature"
print(f"Cloak: {cloak.name}   Body: {body.name}   Arm: {arm.name}")

scn = bpy.context.scene

# --------------------------------------------------------
# Pre-roll: the cloth needs a few frames to settle.
# Extend frame_start BACKWARDS by 30 frames. The walking action starts at its
# original frame_start; during pre-roll the armature sits at frame 0 of the
# anim but the cloak settles into drape.
# --------------------------------------------------------
action = arm.animation_data.action
walk_first = int(action.frame_range[0])
walk_last = int(action.frame_range[1])
PREROLL = 30
sim_first = walk_first - PREROLL
sim_last = walk_last + 2   # a couple tail frames so the last walk frame isn't the last sim frame

scn.frame_start = sim_first
scn.frame_end = sim_last
print(f"Walk frames: {walk_first}..{walk_last}    Sim frames: {sim_first}..{sim_last}")

# --------------------------------------------------------
# Collision modifier on body mesh
# --------------------------------------------------------
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
bpy.context.view_layer.objects.active = body
# Add Collision if not present
if "Collision" not in [m.type for m in body.modifiers]:
    body.modifiers.new(name="Collision", type="COLLISION")
col_settings = body.collision
col_settings.thickness_outer = 0.010   # 1 cm outer margin — conservative, avoids tunneling
col_settings.thickness_inner = 0.010
col_settings.damping = 0.30            # soften bounce
col_settings.cloth_friction = 5.0      # moderate friction against cloth
col_settings.stickiness = 0.0
print(f"Body collision: thickness_outer={col_settings.thickness_outer} damping={col_settings.damping}")

# --------------------------------------------------------
# Cloth modifier on cloak
# --------------------------------------------------------
bpy.ops.object.select_all(action="DESELECT")
cloak.select_set(True)
bpy.context.view_layer.objects.active = cloak

# Remove any existing cloth modifier
for m in list(cloak.modifiers):
    if m.type == "CLOTH":
        cloak.modifiers.remove(m)

cloth_mod = cloak.modifiers.new(name="Cloth", type="CLOTH")
cs = cloth_mod.settings
cc = cloth_mod.collision_settings

# Preset-ish tuning — cinematic drape, not linen, not silk.
# Blender 4.2 cloth settings reference:
# https://docs.blender.org/manual/en/4.2/physics/cloth/settings/physical_properties.html

cs.quality = 10                   # iterations per frame (higher = more stable)
cs.mass = 0.3                     # kg per vert (default is 0.3; heavier drapes slower)
cs.tension_stiffness = 40.0       # resistance to stretching
cs.compression_stiffness = 20.0
cs.shear_stiffness = 10.0
cs.bending_stiffness = 2.5        # moderate bend — leather-ish weight (not paper, not silk)
cs.tension_damping = 25.0
cs.compression_damping = 25.0
cs.shear_damping = 15.0
cs.bending_damping = 1.0
cs.air_damping = 1.2               # slight air drag to smooth motion
cs.effector_weights.gravity = 1.0
# Default internal/pressure = 0 (no pressure)
cs.use_pressure = False
cs.use_internal_springs = False

# PIN GROUP — this is what keeps the cloak's collar attached to the character.
cs.vertex_group_mass = "pin_ring"
cs.pin_stiffness = 15.0

# Collision
cc.use_collision = True
cc.collision_quality = 4           # higher = fewer penetrations at cost of sim time
cc.distance_min = 0.008            # cloth tries to stay 8mm off body
cc.damping = 0.25
cc.self_distance_min = 0.006
cc.use_self_collision = True
cc.self_friction = 5.0
cc.friction = 5.0

print(f"Cloth: mass={cs.mass} bending={cs.bending_stiffness} pin_stiffness={cs.pin_stiffness}")
print(f"Collision: distance_min={cc.distance_min} self_collide={cc.use_self_collision}")

# Set cache frame range
cache = cloth_mod.point_cache
cache.frame_start = sim_first
cache.frame_end = sim_last
cache.name = "cloak_cache"

# --------------------------------------------------------
# Ambient WIND force field — thematic; keeps cloak from reading limp
# --------------------------------------------------------
# Direction: roughly backward-and-up from the character's rear-bottom.
# Character faces -Y, so "from behind" = wind blowing FROM +Y TOWARD -Y
# (i.e. wind pushes cloak AWAY from character's back toward the floor/feet).
# That's wrong — we want wind to push cape OUT behind him, which is +Y direction.
# So wind source must be at -Y (in front of character, slightly below), blowing toward +Y.
# But we want the cloak to float/trail, not be blown flat-back. A mild up-back wind is best.
bpy.ops.object.effector_add(type="WIND", location=(0.0, -1.5, 0.8))
wind = bpy.context.object
wind.name = "cloak_wind"
# Effector's "force direction" is its +Z axis (after rotation).
# Rotate so +Z points toward +Y and slightly up (+Z world).
# From -Y position, we want wind vector pointing away from -Y toward +Y and slightly up.
# Easier: set rotation so local Z-axis = (0, 0.8, 0.6) normalised.
# Rotation to align local +Z with target direction:
import mathutils
target_dir = mathutils.Vector((0.0, 0.8, 0.6)).normalized()
wind.rotation_mode = "QUATERNION"
wind.rotation_quaternion = target_dir.to_track_quat("Z", "Y")

ws = wind.field
ws.strength = 8.0          # m/s-ish — a gentle drift; in Blender this is not in real m/s, it's unitless "strength"
ws.flow = 0.4              # ambient flow component
ws.noise = 0.1             # small amount of noise on the wind itself
ws.seed = 7
ws.falloff_type = "TUBE"
ws.use_max_distance = True
ws.distance_max = 6.0
print(f"Wind: strength={ws.strength} flow={ws.flow}")

# --------------------------------------------------------
# Subtle TURBULENCE for organic variation
# --------------------------------------------------------
bpy.ops.object.effector_add(type="TURBULENCE", location=(0.0, 0.5, 1.0))
turb = bpy.context.object
turb.name = "cloak_turbulence"
ts = turb.field
ts.strength = 3.0
ts.size = 1.4
ts.flow = 0.0
ts.noise = 0.0
ts.use_max_distance = True
ts.distance_max = 4.0
print(f"Turbulence: strength={ts.strength} size={ts.size}")

# --------------------------------------------------------
# Save pre-bake, then bake
# --------------------------------------------------------
pre_bake_blend = os.path.join(os.path.dirname(OUT_BLEND), "cloaked_walking_prebake.blend")
bpy.ops.wm.save_as_mainfile(filepath=pre_bake_blend)
print(f"Saved pre-bake blend: {pre_bake_blend}")

# Bake the cloth sim
print()
print("=" * 70); print("Baking cloth simulation..."); print("=" * 70)

# Override context for point-cache bake.
# In Blender 4.x the proper way is bpy.ops.ptcache.bake with a temporary_override.
scn.frame_set(sim_first)
bpy.context.view_layer.update()

# Select cloak so ptcache.bake targets the right cache
bpy.ops.object.select_all(action="DESELECT")
cloak.select_set(True)
bpy.context.view_layer.objects.active = cloak

# Use the scene override with the cloth's point cache.
# Blender 4.2: ptcache.bake needs point_cache context set.
# Safer approach: call bpy.ops.ptcache.bake_all to bake all point caches in scene.
print("Calling bpy.ops.ptcache.bake_all(bake=True) ...")
try:
    bpy.ops.ptcache.bake_all(bake=True)
    print("Bake complete.")
except Exception as e:
    print(f"bake_all failed ({e}), falling back to frame-by-frame evaluation")
    # Fallback: walk the frames; Blender will fill the cache on-the-fly.
    for f in range(sim_first, sim_last + 1):
        scn.frame_set(f)
    print("Frame-by-frame cache fill complete.")

# --------------------------------------------------------
# Save baked
# --------------------------------------------------------
bpy.ops.wm.save_as_mainfile(filepath=OUT_BLEND)
print(f"Saved baked blend: {OUT_BLEND}")

# Diagnostic: sample the cloak mesh at mid-walk and check it's non-degenerate
mid_frame = (walk_first + walk_last) // 2
scn.frame_set(mid_frame)
bpy.context.view_layer.update()

dg = bpy.context.evaluated_depsgraph_get()
ev = cloak.evaluated_get(dg)
me = ev.to_mesh()
xs = [v.co.x for v in me.vertices]
ys = [v.co.y for v in me.vertices]
zs = [v.co.z for v in me.vertices]
print(f"Cloak mid-walk (frame {mid_frame}): "
      f"x=[{min(xs):.2f}..{max(xs):.2f}] "
      f"y=[{min(ys):.2f}..{max(ys):.2f}] "
      f"z=[{min(zs):.2f}..{max(zs):.2f}]")
# NaN check
nan_cnt = sum(1 for v in me.vertices
              for c in (v.co.x, v.co.y, v.co.z) if not (c == c))
print(f"NaN verts: {nan_cnt}")
ev.to_mesh_clear()

print()
print("=" * 70); print("PHASE 3 COMPLETE"); print("=" * 70)
