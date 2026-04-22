---
skill: model_visor
difficulty: intermediate
prerequisites: [sculpt_helmet_shell, author_matxml_from_blender]
status: template — sources pending (authored 2026-04-21 as exemplar)
---

## Goal

Model a faceted angular visor slit for a helmet — the thin emissive horizontal strip where the eyes would be on a Halo Mjolnir / Berserker-style helmet. Result is a separate mesh with its own emissive material, positioned on the helmet shell's forehead/eye band.

## Sources (5+ required before this guide is `complete`)

> **This is an exemplar template.** The agent session that first executes this skill fills this section with 5+ real video tutorial links before following the steps. Format:
> `N. [Channel — Title](https://youtube.com/watch?v=...) — YEAR — one-line note on why this source`

1. *(pending — search "Blender helmet visor modeling tutorial")*
2. *(pending — search "Blender emissive material visor glowing strip")*
3. *(pending — search "Halo Mjolnir visor model 3D tutorial")*
4. *(pending — search "Blender hard surface helmet visor")*
5. *(pending — search "sci-fi helmet visor Blender workflow")*

## Consensus ordered steps

> **Template skeleton — fill in the consensus from the sources above when this skill is first executed.**

1. Enter Object Mode (`Tab`).
2. Duplicate the helmet shell mesh, suffix the duplicate `_visor_source`, hide the original.
3. Enter Edit Mode on the duplicate (`Tab`). Select the forehead/eye-band face loop (`Alt+click`).
4. `P` → `Selection` to separate the band into its own object.
5. Return to Object Mode, select the new visor object.
6. Enter Edit Mode. `Alt+E` → Extrude along normal, -0.01m (inward). This creates thickness without adding thickness to the front face.
7. Select the front face loop, `I` → inset by 0.008m.
8. `E` → extrude the insets outward 0.002m — this is the lip that catches rim light.
9. Apply `Bevel` modifier: 0.0015m width, 2 segments, limit by angle 30°.
10. Apply `Solidify` modifier: 0.003m thickness, offset 0 (symmetric).
11. Apply `Weighted Normal` modifier (threshold 0.01) so the facets read crisp.
12. In the shader editor, create a new material `visor_emissive`. Nodes: `Emission` (color warm-white `R:1.0 G:0.95 B:0.55`, strength 4.0) → `Material Output`. For Fantasy Etherealism, keep emission warm, not neon.
13. Save the authoring `.blend` (`Ctrl+S`).

## Blender 2.93 `bpy` equivalents

| Step | `bpy` call |
| --- | --- |
| 1 | `bpy.ops.object.mode_set(mode='OBJECT')` |
| 2 | `bpy.ops.object.duplicate()`; rename via `obj.name = "helmet_visor_source"`; hide via `orig.hide_set(True)` |
| 3-4 | Enter edit, select via `bmesh` face iteration + `select` flags, then `bpy.ops.mesh.separate(type='SELECTED')` |
| 6 | `bpy.ops.mesh.extrude_region_move(TRANSFORM_OT_translate={"value":(0,0,-0.01)})` (along the face normal, approximated) |
| 7 | `bpy.ops.mesh.inset_faces(thickness=0.008)` |
| 8 | `bpy.ops.mesh.extrude_region_move(TRANSFORM_OT_translate={"value":(0,0,0.002)})` |
| 9 | `obj.modifiers.new("Bevel", 'BEVEL')` with `.width = 0.0015`, `.segments = 2`, `.limit_method = 'ANGLE'`, `.angle_limit = 0.523` (30°) |
| 10 | `obj.modifiers.new("Solidify", 'SOLIDIFY')` with `.thickness = 0.003`, `.offset = 0.0` |
| 11 | `obj.modifiers.new("WeightedNormal", 'WEIGHTED_NORMAL')` with `.weight_threshold = 0.01` |
| 12 | Build nodes via `mat.node_tree.nodes.new(...)` and `links.new(...)`; see `render_hero_shot_etherealism` for node code |
| 13 | `bpy.ops.wm.save_as_mainfile(filepath=...)` |

## Gotchas

- *(pending — fill with gotchas as tutorials are studied and steps executed)*
- **Face loop extraction** — `P` → `Selection` in 2.93 occasionally fails if the selection isn't a closed face loop; validate with `bmesh.ops.edgeloop_exterior` before separating.
- **Solidify offset** — if offset ≠ 0, the visor shifts into or out of the helmet shell and shadows incorrectly. Use symmetric offset for an isolated visor object.
- **Emissive strength** — Eevee clamps bloom around 10+; for Fantasy Etherealism the visor should glow but not blow out. Strength 3-5 is the sweet spot.

## Post-task update log

(Appended by sessions that use this skill. Newest on top.)

- *(none yet)*
