# Skill — Attach a cloak mesh to a humanoid rig so the pin ring follows the chest

**Target tool:** Blender 4.2 bone parenting (`bpy.ops.object.parent_set(type="BONE")`).
**Status:** authored from single execution 2026-04-22. Single-source, high execution confidence.
**Applies when:** you have a character armature + a cloak mesh with a designated top-ring pin vertex group, and you want the cloth sim's Pin constraint to track the character's chest through the walk cycle.

## The choice

Three valid attachment strategies for a cloak:

1. **Bone Parent (the one this guide uses).** Parent the whole cloak object to a single chest/spine bone. The cloak's object transform = bone's world transform (plus keep_transform offset). Pinned verts move with the chest. Fast, scripted in 5 lines.
2. **Armature modifier + manual weight paint.** Treat the cloak as a skinned mesh, weight-paint the top ring verts to the chest bone at 1.0 weight, other verts at 0. More setup time; better for multi-bone pin distribution (e.g. pin the shoulder verts to their respective shoulders so the pin ring deforms with shrugs).
3. **Vertex Parent.** Parent the cloak to a single vertex on the body mesh. Not recommended for rigged characters because you can't easily account for the armature deforming that vertex — you get a double-transform feel.

For a rigid pin-ring that follows the torso rigidly, Bone Parent is the right pick. For a cloak that needs to deform its pin ring (e.g. a hood collar that moves when the character looks up), Armature modifier wins.

## Ordered procedure — Bone Parent

1. **Select the armature, enter Pose Mode, pick the target bone as active.**
   ```python
   arm = bpy.data.objects["Armature"]
   bpy.ops.object.select_all(action="DESELECT")
   arm.select_set(True); bpy.context.view_layer.objects.active = arm
   bpy.ops.object.mode_set(mode="POSE")
   arm.data.bones.active = arm.data.bones["Spine"]
   bpy.ops.object.mode_set(mode="OBJECT")
   ```
2. **Select the cloak first, then shift-select the armature (armature must be active).**
   ```python
   bpy.ops.object.select_all(action="DESELECT")
   cloak.select_set(True)
   arm.select_set(True)
   bpy.context.view_layer.objects.active = arm
   ```
3. **Call parent_set with `type="BONE"` and `keep_transform=True`.**
   ```python
   bpy.ops.object.parent_set(type="BONE", keep_transform=True)
   ```
4. **Verify:** the cloak's `parent` is now the armature, and its `parent_bone` is "Spine". In the viewport, moving the armature around also moves the cloak.

## Which bone to pick

For a chest-hanging cape, the bone closest in world-space Z to the pin-ring top vertices. For the Obsidian Sentinel:
- Pin-ring top at world Z ≈ 1.39 m.
- Candidate bones' world Z: Spine=1.31, Spine01=1.20, Spine02=1.08, neck=1.39, Head=1.46.
- `Spine` is closest to the pin ring without being IN the neck bone (which would look weird — the cloak transform would flip whenever the character looks down). Chose `Spine`.

Rule of thumb: pick the HIGHEST chest bone that is NOT the neck or head. The chest is generally rotationally stable during a walk; the neck/head can nod/turn, which would cause the cape to nod/turn with them (wrong feel).

## Gotchas hit in real execution

- **`keep_transform=True` matters.** Without it, the cloak's object-space coords are reinterpreted relative to the bone's local frame — and the bone's local frame might not be at world origin. You end up with the cloak teleporting to some weird offset at the parenting step. `keep_transform=True` bakes the current world-space position into the resulting object transform.
- **Active object during parent_set must be the PARENT (armature), not the child.** Blender infers "parent = active, children = selected-but-not-active." Easy to get backwards in a script; the call silently parents nothing or parents the armature to the cloak.
- **Pose Mode ops require the armature to be active AT THE TIME of the mode switch.** If you deselect the armature before calling `bpy.ops.object.mode_set(mode="POSE")`, Blender rejects the mode switch.
- **The cloak still needs its own Cloth modifier AFTER parenting.** Parenting only dictates how the object moves through space. It doesn't provide the cloth-sim constraint. The pin-ring vertex group + Cloth modifier's `vertex_group_mass` setting is what actually glues the top ring to the (now-bone-parented) object position.

## Alternative worth considering (not used here)

- **Dual-parenting for a hooded cape:** bone-parent the object to `Spine`, and additionally weight-paint the TOP-MOST cape verts (the "hood" lift) to the `Head` bone via an Armature modifier. Result: the cloak body follows the chest rigidly, but the hood silhouette turns with the head. Not tried yet but worth prototyping if a future cloak needs head-following.

## Open question

Didn't test: what happens if the character's armature scales (e.g. a power-up makes him grow)? The bone parent with keep_transform would track the scale, but the cloth sim's pin stiffness is tuned for a specific scale. Probably needs re-bake, but the symptom-pattern isn't characterized yet.

## Sources consulted (authored from execution)

- `demo/showcase/assets/obsidian_sentinel_cloaked/scripts/phase2_cloak_geometry.py` (lines near `bpy.ops.object.parent_set`)
- Blender 4.2 Python API — `bpy.ops.object.parent_set`: https://docs.blender.org/api/current/bpy.ops.object.html#bpy.ops.object.parent_set
- Blender manual — Parenting: https://docs.blender.org/manual/en/4.2/scene_layout/object/properties/relations/parents.html
