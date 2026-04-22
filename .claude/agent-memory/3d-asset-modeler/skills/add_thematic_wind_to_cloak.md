# Skill — Add thematic wind + turbulence to a cape/cloak cloth sim

**Target tool:** Blender 4.2 force fields (Wind + Turbulence effectors).
**Status:** authored from single execution 2026-04-22. High real-execution confidence, low source confidence.
**Applies when:** you've got a cloth-sim'd cloak and it reads LIMP at slow walk/idle speeds. Adding ambient wind makes it flow visually even when the character isn't moving fast.

## Why this exists

A cloth sim with only gravity + collision + a pin group produces a cape that, at slow cadence, falls flat against the character's back and does nothing dramatic. Video games and cinema almost always cheat with a constant low-amplitude breeze plus a little turbulence — the cape trails, the hem sways, the silhouette stays moody. This is "thematic wind": it's not tied to any in-world weather; it's a stylistic constant.

## Ordered procedure

1. **Add a Wind force field upstream of the cape.** Upstream = on the side the wind is blowing FROM. For a cape meant to trail BEHIND the figure, place the wind origin IN FRONT OF the figure, blowing toward the back. (Character faces -Y → wind origin at -Y, direction +Y. Up-angle +30-45° is good for a floaty trailing look.)

2. **Orient the wind's force direction.** Blender's Wind effector blows along the object's LOCAL +Z axis. To point it at `target_dir` in world space:
   ```python
   from mathutils import Vector
   target_dir = Vector((0, 0.8, 0.6)).normalized()
   wind.rotation_mode = "QUATERNION"
   wind.rotation_quaternion = target_dir.to_track_quat("Z", "Y")
   ```

3. **Tune wind strength LOW.** The field `strength` parameter is unitless (not m/s) and roughly acts like a force multiplier. Values 6-12 produce visible-but-not-frantic cape motion. Anything above 20 and the cape flaps like a flag in a storm.

4. **Add `flow = 0.3-0.5`.** This adds a constant-velocity component, which the solver uses as the ambient air velocity the cloth tries to match. Gives the cape a baseline "always trailing" look even when the character is perfectly still.

5. **Add a small `noise = 0.1` with a deterministic `seed`.** Produces low-amplitude variation in the wind itself — the cape will gently shift rather than looking flat-force-driven.

6. **Limit the wind's reach.** `falloff_type = "TUBE"` + `use_max_distance = True` + `distance_max = 6.0` prevents the wind from affecting other cloth objects in the scene or extending beyond the intended character's bubble.

7. **Add a Turbulence force field on top.** Turbulence is 3D Perlin-ish noise that advects the cloth in x/y/z randomly. Use it sparingly — it breaks the visual symmetry of the wind-driven flow and adds "alive" variation. Good values: `strength = 2-4`, `size = 1.0-1.8`. Position at the cape's middle, limit distance to ~4 m.

8. **Bake and visually verify.** The telltale sign that wind is working: between two adjacent mid-walk frames, the cape hem moves measurably (several cm) in a direction other than pure gravity-fall. If the cape reads completely still between frames, the wind is too weak or aimed wrong.

## bpy snippet

```python
import bpy, math, mathutils

# WIND — constant drift from front to behind-and-up
bpy.ops.object.effector_add(type="WIND", location=(0.0, -1.5, 0.8))
wind = bpy.context.object
target_dir = mathutils.Vector((0.0, 0.8, 0.6)).normalized()
wind.rotation_mode = "QUATERNION"
wind.rotation_quaternion = target_dir.to_track_quat("Z", "Y")
ws = wind.field
ws.strength = 8.0
ws.flow = 0.4
ws.noise = 0.1
ws.seed = 7
ws.falloff_type = "TUBE"
ws.use_max_distance = True
ws.distance_max = 6.0

# TURBULENCE — organic variation on top of wind
bpy.ops.object.effector_add(type="TURBULENCE", location=(0, 0.5, 1.0))
turb = bpy.context.object
ts = turb.field
ts.strength = 3.0
ts.size = 1.4
ts.use_max_distance = True
ts.distance_max = 4.0
```

## Gotchas hit in real execution

- **Wind direction gotcha.** A Blender Wind effector blows along the effector's local +Z axis — after rotation. If you place a Wind object with default rotation, it blows straight UP, not in the direction you might think. Always explicitly set `rotation_quaternion` using `to_track_quat("Z", ...)`. Verify by eyeballing the force-field visualization arrow in the viewport.
- **Wind from BEHIND vs wind from IN-FRONT.** Intuition says "blow wind from behind so the cape floats behind the figure." This is backwards. Wind pushes AWAY from its source in the direction of its force. To push the cape backward (away from the body), the wind source needs to be IN FRONT of the body, force vector pointing BACKWARD. If you put wind behind the character pointing forward, you blow the cape UNDER the character's armpit and forward — exactly the wrong look.
- **Don't confuse `strength` with a velocity.** Many AI-generated tutorials treat `strength` as m/s. It's not. It's a multiplier on the effector force applied to each cloth particle per substep. Use 6-12 for cinematic and calibrate visually.
- **Ambient flow component (`flow`) is what makes it not look like a fan.** If you only crank `strength` the cape has an obvious "blown by a point source" profile — rippling hardest where the cone axis points. Adding `flow = 0.3-0.5` spreads the force uniformly through the field's bounding volume, which reads as "air is moving."
- **Turbulence at high strength destroys the pin constraint.** Keep turbulence under `strength = 5`; above that the cloth solver starts rejecting the pin group's position constraint at the hem, and you see the entire cape bob up and down unnaturally.
- **If you forget the turbulence `seed` parameter, Blender bakes a different sim on every restart.** Set `seed` explicitly to a fixed value if you want bit-exact rebakes across machines.

## Open question

Didn't test whether the wind effector interacts with cloth `air_damping` in a coupled way. Could likely reduce bake time by raising `air_damping` if you're also running wind, since the cloth has an external air-velocity reference. Not chased.

## Sources consulted (authored from execution)

- `demo/showcase/assets/obsidian_sentinel_cloaked/scripts/phase3_cloth_sim.py`
- Blender 4.2 manual — Force Fields: https://docs.blender.org/manual/en/4.2/physics/forces/force_fields/types.html
- General CG industry practice — "ambient wind + turbulence" is standard on any cinematic cape (Assassin's Creed, Dark Souls, Berserk adaptations)
