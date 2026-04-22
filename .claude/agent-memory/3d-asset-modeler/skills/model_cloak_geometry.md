# Skill — Model cloak / cape geometry for a humanoid

**Target tool:** Blender 4.2 (works in 2.93 with minor `bpy` renames; not tested there).
**Status:** authored from single execution 2026-04-22 on the Meshy Obsidian Sentinel character. Low source confidence, high real-execution confidence.
**Applies when:** adding a back-draping cape or hooded cloak to an already-rigged humanoid where you plan to run a cloth simulation.

## Ordered procedure

1. **Read the target character's shoulder + neck pin-ring world-space positions.** Do NOT eyeball. The pin-ring is the anchor line that will be fully pinned by the cloth sim; it must match the character's chest height within ~1 cm. For the Obsidian Sentinel: neck head=(0, 0.06, 1.39), L/R shoulder heads at Z=1.40, character faces -Y.

2. **Decide where the cape arc opens.** For a dramatic flowing-behind cape, open the arc at the FRONT (110-130° gap, = 230-250° arc wrapping the back). For a more enclosing "mage/wizard" look, close to 30° front gap (= 330° arc, nearly a full cylinder). For a shoulder-to-shoulder semi-circle only ("superhero" back cape) use 180° arc centered behind.

3. **Choose topology density for the cloth solver.** Target ~700-1200 verts total, ≥24 arc segments, ≥20 rings top-to-bottom, quads only, even density. Uneven density causes cloth tearing and visible stretch seams. Don't go below 20×20 — the solver will look blocky.

4. **Build a parameterized grid in polar coords.** For each ring `i` (0=top/pinned, N-1=bottom/hem), for each arc segment `j`, compute (x, y, z):
   - radius grows linearly from TOP_RADIUS (~0.20 m — snug shoulder ring) to BOTTOM_RADIUS (~0.55 m — flared hem).
   - z interpolates from TOP_Z (just below neck, ~1.38 m) to BOTTOM_Z (~0.06 m — just above floor).
   - angle φ(j) spans the chosen arc centered on the character's backward axis.
   - Add a small BACKWARD_OFFSET (~-0.08 m) on the rear Y so the first sim frame isn't intersecting the body's spine.
   - Add a COLLAR_LIFT on the top 2-3 rings so the cape suggests a stand-up collar (hood alternative).

5. **Generate quads linking ring i→i+1 and segment j→j+1.** Standard gridwise, 4-vert quads.

6. **Define a pin-ring vertex group.** Top ring (i=0) at weight 1.0, second ring (i=1) at weight 0.4 for a softer attachment band. Cloth's Pin Group will reference this.

7. **Parent the cloak object to the character's chest/spine bone** with `bpy.ops.object.parent_set(type="BONE", keep_transform=True)`. The pinned vertices now follow the bone, so the pin constraint in the cloth sim needs zero extra scripting.

8. **Save the .blend BEFORE running cloth sim.** Name it `cloak.blend` or similar. Keep Phase 2 output separate from Phase 3 baked output — if the sim misbehaves, you can tune from here without re-authoring geometry.

## bpy snippet (abbreviated — full script in `demo/showcase/assets/obsidian_sentinel_cloaked/scripts/phase2_cloak_geometry.py`)

```python
import bpy, bmesh, math, mathutils

N_RINGS, N_ARCS = 26, 34
ARC_DEG = 250.0
TOP_Z, BOTTOM_Z = 1.38, 0.06
TOP_R, BOT_R = 0.23, 0.55

verts = []
idx = {}
for i in range(N_RINGS):
    t = i / (N_RINGS - 1)
    r = TOP_R + (BOT_R - TOP_R) * (t ** 1.2)
    z = TOP_Z * (1 - t) + BOTTOM_Z * t
    for j in range(N_ARCS):
        u = j / (N_ARCS - 1)
        phi = math.radians(90.0 - (u - 0.5) * ARC_DEG)  # center arc on +Y (backward if char faces -Y)
        verts.append((r * math.cos(phi), r * math.sin(phi) - 0.08 * (1 - t), z))
        idx[(i, j)] = len(verts) - 1

faces = [(idx[(i,j)], idx[(i,j+1)], idx[(i+1,j+1)], idx[(i+1,j)])
         for i in range(N_RINGS - 1) for j in range(N_ARCS - 1)]

mesh = bpy.data.meshes.new("cloak_mesh")
mesh.from_pydata(verts, [], faces)
cloak = bpy.data.objects.new("cloak", mesh)
bpy.context.collection.objects.link(cloak)

# Pin vertex group
vg = cloak.vertex_groups.new(name="pin_ring")
vg.add([idx[(0, j)] for j in range(N_ARCS)], 1.0, "REPLACE")
vg.add([idx[(1, j)] for j in range(N_ARCS)], 0.4, "REPLACE")

# Parent to Spine bone
arm = bpy.data.objects["Armature"]
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="POSE")
arm.data.bones.active = arm.data.bones["Spine"]
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.object.select_all(action="DESELECT")
cloak.select_set(True); arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type="BONE", keep_transform=True)
```

## Gotchas hit in real execution

- **"Which direction is backward?"** — depends on how the glTF import rotated the source. The Meshy-generated rig exported the character with forward = +Y in glTF (standard), but Blender's glTF importer rotates -90° about X during import → character now faces **-Y in Blender**. If you assume +Y backward without checking, your arc opens on the wrong side. Always verify with a bone like `headfront` (if present) or inspect the mesh's top-15%Z verts by sign of Y before laying out the arc.
- **Pin-ring width vs shoulder-arm spacing.** If TOP_RADIUS > the distance from the spine to the upper-arm root (typically 0.18-0.22 m on a 1.8 m figure), the pin ring's top verts sit INSIDE the deltoid volume, and cloth sim starts each frame with an interpenetration. Keep TOP_RADIUS ≤ spine-to-upper-arm distance and let the arc curve wrap behind the shoulder rather than through it.
- **Hood on a horned helmet fails.** If the character has horns, forks, antlers, or tall spikes on the helmet, don't author a geometric hood — the hood collides with the protrusions every frame and the sim either blows up or stretches catastrophically. Substitute: raise the top 2-3 rings of the cape 5-10 cm higher than the neck pin ring to suggest a stand-up collar. Visually reads as a hooded cape; cloth solver stays sane.
- **Parent "Spine" vs parent "root" matters.** Parenting the cloak's object transform to the root bone means the cape's pinned ring stays at a fixed world-space Z (around 1.4 m) even as the character's chest bobs during walk — the chest then swims independently of the pin ring, which looks like a broken constraint. Parent to the highest torso bone you have (`Spine`, `Spine01`, `chest`) so the pin ring rides the chest.

## Open question

Didn't investigate: Blender's "Adaptive Subdivision" modifier on cloth. Could we author a 600-vert mesh and let the solver densify it for rendering? Not tried because 884 verts was already fast enough to sim.

## Sources consulted (this skill was authored from execution, not tutorials)

- `demo/showcase/assets/obsidian_sentinel_cloaked/scripts/phase2_cloak_geometry.py` — full working script
- `demo/showcase/assets/obsidian_sentinel_cloaked/BUILD_LOG.md` — end-to-end log
- Blender 4.2 manual, Cloth / Physical Properties: https://docs.blender.org/manual/en/4.2/physics/cloth/settings/physical_properties.html (for adjacent section on pinning + verts-per-cm recommendations)
