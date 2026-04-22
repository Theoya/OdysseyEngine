---
skill: animate_walk_cycle
difficulty: intermediate
prerequisites: [skin_base_to_skeleton, pose_character_static]
target_blender: 4.2 LTS (portable at C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\blender.exe) — but the output format is engine-neutral, authoring version flexible
status: complete — authored 2026-04-21
---

## Goal

Author a looping walk cycle on a rigged humanoid and export it as an OdysseyEngine `.anim.xml` targeting the 19-bone skeleton at `demo/fps_humanoid/assets/humanoid.skeleton.xml`.

The canonical target here is the Richard Williams "contact method" — a 4-key cycle (contact, down, passing, up) per step, mirrored for the opposite leg, giving 8 core poses per full cycle + a return-to-contact at the end for the loop. That's the standard the rest of the industry teaches against.

## Sources (5+ required — collected 2026-04-21)

1. [Richard Williams — The Animator's Survival Kit: Walk Cycle (Billy Hook's notes)](https://billyhookpenwithdigital.wordpress.com/2019/02/22/walk-cycle-animation/) — canonical student breakdown of the 4-key method with diagrams of contact / down / passing / up.
2. [Monmouth Animation Instruction: Walk Cycle](https://animation.monmouth.edu/instruct/animation/walk-cycle/) — university-course walk of the same 4-key structure with pelvis/chest counter-rotation notes.
3. [GarageFarm — Walk Cycle: Easy Steps to Animate Walking Animation for Beginners](https://garagefarm.net/blog/walk-cycle-easy-steps-to-animate-walking-animation-for-beginners) — applied Blender walkthrough: hip S-curve, arm counter-swing amplitude, toe roll on push-off.
4. [Dark Skies — Blender Walking Animation guide](https://darkskiesfilm.com/how-to-make-a-walking-animation-in-blender/) — Blender-specific tips for keyframe interpolation (constant → bezier) and the Graph Editor cyclic modifier workflow.
5. [Blender Manual — F-Curve Modifiers (Cycles)](https://docs.blender.org/manual/en/latest/editors/graph_editor/fcurves/modifiers.html) — canonical reference on `Cycles` modifier with `Repeat Motion` vs `Repeat with Offset`, required for infinite loops via Shift+E.
6. [BLENDER — Using the F-Curve Cycle modifier (YouTube)](https://www.youtube.com/watch?v=Dgi3cSZA3bo) — 4-minute demo showing cyclic f-curve modifier on all selected fcurves for full-body loop.
7. [iRender — Advanced Tips for Walk Cycles in Blender](https://irendering.net/advanced-tips-for-walk-cycles-in-blender/) — covers foot-slide diagnosis + root-motion-vs-in-place decision.
8. [Feet Skating/Sliding Fix — Blender Motion Capture (YouTube)](https://www.youtube.com/watch?v=bmapPWIR4L0) — IK foot target locking and key interpolation constant-hold pattern.
9. *Internal source:* `demo/fps_humanoid/assets/walk_cycle.anim.xml` — the shipping engine reference. 0.8-second cycle, 5 keyframes per track (t=0, 0.2, 0.4, 0.6, 0.8), 7 animated bones (`root`, `spine`, `upper_leg_l/r`, `lower_leg_l/r`, `upper_arm_l/r`). In-place (no root XZ translation). Root Y bobs 2cm (0.95→0.97→0.95→0.97→0.95). Upper-leg rotation amplitude ±0.2 rad (~11°). Knee bend peaks at 0.25 rad (~14°) on passing frames. Arm counter-swing ±0.15 rad (~8°). This is the target when a new walk is authored — match cadence, tune style.

## Consensus 4-key structure (Richard Williams)

| Frame | Phase | What's happening | Root Y (relative to rest 0.95) | Pelvis roll |
| --- | --- | --- | --- | --- |
| 1 | **Contact (R)** | Right foot strikes forward, left foot is pushing off behind. Legs at widest stride. Arms counter: left arm forward, right back. | **-0.02m** (slight absorb) | 0 |
| 9 | **Down (R)** | Full weight on right leg, knee bent. Hips at lowest. | **-0.04m** (lowest) | +3° toward right (planted) leg |
| 17 | **Passing (R→L)** | Right leg straightens. Left knee lifts and passes under pelvis. Arms crossing neutral. | **0.0m** (neutral) | 0 |
| 25 | **Up (R)** | Right toe pushes off, pelvis rises. Left foot approaches contact. | **+0.01m** (highest) | -1° (preparing left contact) |
| 33 | **Contact (L)** | Mirror of frame 1. Left foot forward, right back. | -0.02m | 0 |
| ...continuing... | down (L) at 41, passing (L→R) at 49, up (L) at 57, back to contact R at 65 — OR trim to one step and cyclic-loop with mirroring. |

For the stock stick-figure walk, we use a shorter 24-frame loop (1 second at 24fps) with only the R-side keys and rely on the arm + opposite-leg counter-mirroring to sell the cycle. For the heavier Berserk-Halo walk we use a 33-frame loop (1.4s) and a 40-frame variant for the Mk3.

**For the MPFB base walking at "half decent" bar:** reuse the shipping `walk_cycle.anim.xml` (0.8s, 5 keys, targets our 19 bone names exactly). Authoring a fresh cycle only makes sense if the base's proportions drift the existing anim visibly (they shouldn't — we scaled to engine-skeleton 1.8m total).

## Consensus ordered steps (Workflow B.a — **retarget existing anim**)

This is the default for the base humanoid — the engine walk already targets 19 bones, we skinned the base to 19 bones, so no new anim authoring is required.

1. Open the skinned base: `bpy.ops.wm.open_mainfile(filepath="third_party/base_humanoid/<gender>/base_<gender>_skinned.blend")`.
2. Author a small Python loader that **parses `demo/fps_humanoid/assets/walk_cycle.anim.xml`** and applies it as a Blender Action:
   - For each `<track bone="X">`, find the pose bone on the engine armature named X.
   - For each `<key time="T" position="px py pz" rotation="rx ry rz rw"/>`:
     - Set `frame = round(T * fps)` where `fps = 24`.
     - Set `posebone.location = (px, py, pz) - rest_position`.
     - Set `posebone.rotation_quaternion = (rw, rx, ry, rz)` (Blender uses WXYZ order; engine XML is XYZW).
     - `posebone.keyframe_insert(data_path="location", frame=frame)`.
     - `posebone.keyframe_insert(data_path="rotation_quaternion", frame=frame)`.
3. After loading all keys, iterate `armature.animation_data.action.fcurves` and attach a `CYCLES` f-modifier to each with `mode_before='REPEAT'` and `mode_after='REPEAT'`. This makes Blender play the 0.8s anim on infinite loop for preview.
4. Set `scene.frame_start = 1`, `scene.frame_end = 20` (= 0.8s * 24fps, + 1 for closing pose), `scene.render.fps = 24`.
5. Scrub the timeline. **Validate visually:**
   - Feet don't slide (engine walk is in-place, so feet _should_ visibly lift — the mesh feet stay at Y=0 elevation at contact frames, lift by ~5-10cm at passing frames).
   - Knees don't pop sideways (indicates weight-paint error or bone-roll mismatch).
   - Arms counter-swing — left arm forward when right leg forward.
   - Mesh doesn't tear at shoulders/hips (indicates vertex groups not covering the joint or weight-sum < 1.0).
6. Save: `bpy.ops.wm.save_as_mainfile(filepath="third_party/base_humanoid/<gender>/walking.blend")`.
7. Render 4 keyframe shots at frames 1, 8, 16, 24 (contact / down / passing / up) for the `renders/` gallery — see `render_hero_shot_etherealism.md`.

**If the existing anim looks wrong on the base:** don't modify `walk_cycle.anim.xml`. Instead, proceed to Workflow B.b below.

## Consensus ordered steps (Workflow B.b — **author fresh walk on 53-bone MPFB rig**)

Use when retargeting the stock anim produces visible issues (e.g. the base mesh has different leg proportions, or we want a heavier/feminine-coded variant).

1. Open the unscaled, MPFB-rigged mesh: `base_<gender>.blend` — 53-bone `game_engine` rig still attached.
2. Enter Pose Mode on the armature: `bpy.context.view_layer.objects.active = armature; bpy.ops.object.mode_set(mode='POSE')`.
3. Set scene frames 1..33 for a 1.4s heavy walk, or 1..20 for a 0.8s light walk. `scene.render.fps = 24`.
4. **Author keyframes on the MPFB bones** — use Unreal-style names (`thigh_l`, `calf_l`, `upperarm_r`, etc). For each of the 4 contact-method frames (contact, down, passing, up):
   - Select all pose bones (`bpy.ops.pose.select_all(action='SELECT')`).
   - Set the full-body pose (thigh/calf rotations, pelvis roll, spine S-curve, arm counter-swing).
   - Insert a location + rotation_quaternion keyframe on each animated bone.
5. Mirror the pose for the other foot's contact, paste at frame 17: Blender's Pose Library or `bpy.ops.pose.copy()` + `bpy.ops.pose.paste(flipped=True)`.
6. Attach `CYCLES` f-modifier to all fcurves for seamless loop.
7. **Bake the anim down to the 19-bone engine skeleton.** Since the 53 bones pose the mesh via weights but our `.anim.xml` format only references 19 bones, we write a bake script:
   - For each frame in the cycle:
     - Evaluate the MPFB armature's pose.
     - For each engine bone (19), compute its equivalent local-space rotation + translation from the MPFB bone(s) it maps to (see `skin_base_to_skeleton.md` for the mapping table).
     - Write to the engine armature's pose and keyframe.
   - This is the only lossy step — merging `spine_02 + spine_03` into `chest` collapses the spine's 3-joint articulation to 1 joint. Accept the loss for "half decent" bar.
8. Export `.anim.xml` using `write_anim_xml(armature, action, skeleton_bone_order)`:
   ```python
   doc = Element("animation", name="custom_walk", duration=str(duration_s), looping="true")
   for bone_name in engine_bone_names:
       pose_bone = armature.pose.bones[bone_name]
       fcurves = [fc for fc in action.fcurves if pose_bone.path_from_id() in fc.data_path]
       if not fcurves: continue
       track = SubElement(doc, "track", bone=bone_name)
       for frame in sorted(unique_frames(fcurves)):
           t = (frame - 1) / fps
           pos = pose_bone.evaluated_location_at(frame)  # custom eval
           rot_wxyz = pose_bone.evaluated_rotation_at(frame)
           # Engine XML uses XYZW rotation order:
           SubElement(track, "key",
               time=f"{t:.4f}",
               position=f"{pos.x:.4f} {pos.y:.4f} {pos.z:.4f}",
               rotation=f"{rot_wxyz.x:.4f} {rot_wxyz.y:.4f} {rot_wxyz.z:.4f} {rot_wxyz.w:.4f}")
   ```

## Heavy-armor variations (from Berserk-Halo walk)

When the target character reads as armored/heavy:
- Slow cadence by 40-75% (33 or 40-frame cycle vs 20-frame stock).
- Root dip deeper on plant frames (4cm vs 2cm).
- Add a permanent +3° hunch on the chest bone (forward lean).
- Halve the arm-swing amplitude (armor restricts reach — 0.08 rad instead of 0.15).
- Add a 2° shoulder dip counter-rotate per step for weight.

## Blender 4.2 `bpy` equivalents

| Step | `bpy` call |
| --- | --- |
| 1-2 | `bpy.context.view_layer.objects.active = armature`; `bpy.ops.object.mode_set(mode='POSE')` |
| 3 | `bpy.context.scene.frame_start = 1`; `bpy.context.scene.frame_end = 20`; `bpy.context.scene.render.fps = 24` |
| 4-6 | For each bone at each frame: `bone.location = (x,y,z)`; `bone.rotation_quaternion = (w,x,y,z)`; `bone.keyframe_insert(data_path="location", frame=F)`; `bone.keyframe_insert(data_path="rotation_quaternion", frame=F)` |
| cycle loop | `for fc in armature.animation_data.action.fcurves: mod = fc.modifiers.new('CYCLES'); mod.mode_before='REPEAT'; mod.mode_after='REPEAT'` |
| 8 | Custom `write_anim_xml()` walks `action.fcurves`, interpolates per frame, emits engine XML |
| 7 review | Manual review — headless script cannot eyeball this. If the cycle has measurable foot-slide it's a rigor failure; log it honestly. |

## XML format rotation convention (load-bearing)

The engine `.anim.xml` rotation attribute is **XYZW** quaternion order: `rotation="x y z w"`.
Blender's `rotation_quaternion` is **WXYZ** order: `(w, x, y, z)`.

Always convert explicitly:
```python
# Blender → engine XML
w, x, y, z = bone.rotation_quaternion
xml_rotation = f"{x:.4f} {y:.4f} {z:.4f} {w:.4f}"

# Engine XML → Blender
x, y, z, w = map(float, attr.split())
bone.rotation_quaternion = (w, x, y, z)
```

Getting this wrong produces **360° spins on every interpolation** — the animation looks like violent shaking. Always unit-test with identity (0,0,0,1): a rest pose should read `"0 0 0 1"` in XML, NOT `"1 0 0 0"`.

## Foot-slide diagnosis

With the in-place walk cycle, the mesh feet SHOULD stay near Y=0 at contact frames and lift ~5-15cm at passing frames. Foot-slide is usually:

1. **Arm-swing not mirrored** — if arms swing WITH legs instead of against, the hips counter-rotate wrong and the feet cheat.
2. **Skin-weight leaking** — `foot_l` vertices have residual weight to `calf_l`; fix in weight paint.
3. **Passing-frame knee too tall** — lower the knee-lift angle on passing frames.
4. **Cadence-vs-stride mismatch** — If integrating with a root-motion mover, the forward velocity must equal `stride_length / cycle_duration`. Stock engine anim is in-place; root XZ is left alone.

For the "half decent" bar, accept ≤ 3cm horizontal foot drift per cycle. Beyond that, log it and decide between (a) tune the leg rotation, (b) accept as part of the bar, (c) add IK foot target (out of scope for Step 2).

## Gotchas

- **Quaternion sign flips** at keyframe boundaries cause 360° spins during interpolation. After setting each `rotation_quaternion`, check `dot(prev_q, new_q) < 0` and negate if so. Blender's slerp interpolates the "short way" automatically IF the stored quaternions don't have opposite signs; storing an opposite sign on one frame breaks this.
- **Cyclic F-Modifier** is Blender's way to loop in preview; for export, the `.anim.xml` format requires the LAST keyframe to equal the FIRST (or be trimmed). The shipping `walk_cycle.anim.xml` uses `time="0.0"` and `time="0.8"` with identical values — the loop point is authored manually.
- **Foot IK vs FK**: if the rig has IK set up on legs, animate the IK target bones, not the FK joints — otherwise feet slide. If the rig is FK-only (our engine rig IS FK-only), use foot-plant constraints via Graph Editor handles (Constant interpolation on foot translation during contact frames).
- **Root bone as motion carrier**: the 19-bone humanoid uses `root` at Y=0.95 — animate root **Y** (up/down) for weight, NOT chest or spine alone, or the hips will float detached. Do NOT animate root XZ for in-place cycles — let the engine mover code handle translation.
- **Keyframe interpolation default**: Blender defaults to Bezier. For crisp contact poses, set foot-related fcurves to Constant for the plant duration: `fcurve.keyframe_points[i].interpolation = 'CONSTANT'`.
- **Sub-frame timing**: engine `<key time="0.0125"/>` (= frame 0.3 at 24fps) is valid but ugly — round times to whole frames for artist-legibility.
- **MPFB `game_engine` armature has a `Root` bone (capital R) at world origin**. Ignore it in the bake — engine's `root` maps to `pelvis`, as documented in `skin_base_to_skeleton.md`.
- **Blender 4.2 `action.fcurves` is readonly when no animation_data** — first `armature.animation_data_create(); armature.animation_data.action = bpy.data.actions.new("walk_cycle")`.

## Post-task update log

(Appended by sessions that use this skill. Newest on top.)

- 2026-04-21 — skill sources + steps populated. Workflow B.a (retarget existing `walk_cycle.anim.xml` → skinned base) promoted to default for "half decent" bar — we skinned the base to 19 bones with engine-matching names, so the shipping anim drives it via string-matched tracks without re-authoring. Workflow B.b (author fresh on 53-bone MPFB rig) documented as fallback for future heavier walks or style variants.
