# Skill — Set up Blender 4.2 cloth simulation for a character cape

**Target tool:** Blender 4.2.20 LTS (BLENDER_EEVEE_NEXT render engine).
**Status:** authored from single execution 2026-04-22 on the Meshy Obsidian Sentinel. Low source confidence, high real-execution confidence.
**Applies when:** baking a cloth sim on a cape/cloak pinned to a walking humanoid.

## Ordered procedure

1. **Open the .blend with the cloak mesh parented to a chest bone.** See `model_cloak_geometry.md` for authoring. The cloak must have a vertex group (e.g. `pin_ring`) designating the verts that will stay glued to the character.

2. **Add a Collision modifier to the character body mesh.** Settings that worked:
   - `thickness_outer = 0.010` (1 cm exterior margin; prevents tunneling without ballooning the cape off the body)
   - `thickness_inner = 0.010`
   - `damping = 0.30`
   - `cloth_friction = 5.0`
   Don't add Collision to the armature itself — only meshes that have a Collision modifier count as collider surfaces.

3. **Add a Cloth modifier to the cape mesh.** Tuning for cinematic (not silk, not burlap) drape:
   - `quality = 10` (solver iterations per frame — higher = more stable through fast motion, costs sim time)
   - `mass = 0.3` (kg per vert equivalent)
   - `tension_stiffness = 40`, `compression_stiffness = 20`, `shear_stiffness = 10` — moderate stretch resistance
   - `bending_stiffness = 2.5` — "leather-heavy fabric" bend; silk would be 0.5, canvas would be 10+
   - All damping values around 1-25 for smoother motion
   - `air_damping = 1.2` — slight air drag to prevent whip-crack
   - `use_pressure = False`, `use_internal_springs = False` for an open, non-inflating cape

4. **Assign the pin group:**
   - `settings.vertex_group_mass = "pin_ring"`
   - `settings.pin_stiffness = 15.0`
   This is the single most important line of the whole setup. If it's missing, the entire cape falls to the floor as gravity pulls it off the character.

5. **Collision settings on the Cloth modifier:**
   - `collision_quality = 4` (number of collision resolution iterations per substep)
   - `distance_min = 0.008` — cloth tries to stay 8 mm off the body; smaller = tighter fit but more penetrations
   - `use_self_collision = True`, `self_distance_min = 0.006`
   - `friction = 5.0`, `self_friction = 5.0`

6. **Extend the scene frame range backward for PRE-ROLL.** Cloth needs ~30 frames to settle from T-pose into draped rest before the walk starts; otherwise the cape is stretched or bunched at frame 1. `scn.frame_start = walk_start - 30`.

7. **Set the cloth cache frame range to match the extended range.** `cloth_mod.point_cache.frame_start = sim_first` and `frame_end = sim_last`.

8. **Bake with `bpy.ops.ptcache.bake_all(bake=True)`.** Simpler than trying to override context for a single point_cache bake in headless mode. It bakes all point caches in the scene (including any rigid-body caches), which is harmless here.

9. **Save the baked .blend.** The baked state lives in the .blend's point_cache data block. If you don't save, the bake is thrown away on exit.

10. **Verify: open the saved .blend, jump to a mid-walk frame, read the cape mesh bbox.** If any bbox axis is > 10 m or contains NaN, the sim blew up. Re-bake with lower solver quality or higher damping. For the Obsidian Sentinel, mid-walk bbox was x=[-0.27..0.40] y=[-0.26..0.75] z=[0.07..1.47] — sane.

## Gotchas hit in real execution

- **`ptcache.bake_all` doesn't require a specific object selection.** Don't waste time trying to build a `bpy.context.temp_override(...)` for a cloth-specific point cache in headless mode. `bake_all(bake=True)` iterates every point cache in the scene and bakes them in sequence. Good enough.
- **Blender 4.2 doesn't expose `mesh.use_auto_smooth` anymore** — it was replaced by a modifier-based auto-smooth in the Shade Smooth operator. Trying to set `cloak.data.use_auto_smooth = True` raises AttributeError. Wrap in try/except and fall back to `bpy.ops.object.shade_auto_smooth(angle=math.radians(60))`.
- **The default glTF-imported armature has an "Armature" modifier on the body mesh.** When you add a Collision modifier, make sure it's AFTER the Armature modifier in the stack — otherwise collision is computed in the pose-rest frame, not in the deforming frame, and the cloak passes through the body. Blender puts new modifiers at the bottom of the stack by default, which is correct here (Armature first, Collision below).
- **Pre-roll frames with negative numbers confuse some tools.** `scn.frame_start = -30` works in Blender but if you re-render later you'll see "frame -30" in the bake log, which looks wrong but isn't.
- **`cc.use_collision` (on Cloth's collision settings) is on by default in Blender 4.2**, but setting it explicitly is cheap insurance. Forgetting it was fine in our test but setting defensively costs one line.
- **`settings.effector_weights.gravity = 1.0`** — the default is already 1.0, but if you inherited a .blend from someone who reduced gravity for a test, that value persists. Set it explicitly.

## Open question

Didn't test: baking cloth sim against a sub-stepped armature. If the walk cycle has very fast limb motion (sprint, jump), the default 1 substep per frame might not be enough and cloth tunnels through limbs. Setting `scn.rigidbody_world.substeps_per_frame` doesn't affect cloth — need to look into the hidden `quality` interpretation more carefully. Didn't hit this on Walking (slow cycle).

## Sources consulted (authored from execution)

- `demo/showcase/assets/obsidian_sentinel_cloaked/scripts/phase3_cloth_sim.py`
- Blender 4.2 manual — Cloth physics section (https://docs.blender.org/manual/en/4.2/physics/cloth/)
- Empirical tuning against Meshy-generated character + walking animation
