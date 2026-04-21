# 3D Modeling Agent — Persistent Tips

Living playbook for AI-agent-driven 3D asset production in OdysseyEngine.
Keep this doc lean. Trim, reorganize, and deduplicate as tactics land or go stale.

---

## 1. Host-project ground truth (OdysseyEngine, checked 2026-04-21)

- Mesh format is a **descriptor-only XML** (`schemas/mesh.xsd`). It points at one of:
  - `<source format="primitive">box|sphere|cylinder|capsule|ground</source>`
  - `<source format="obj"  path="demo/.../foo.obj"/>` — **loader is wired** (tinyobj in `src/assets/mesh_loader.cpp`). Missing UVs default to (0,0); missing normals default to (0,1,0) — safe to omit.
  - `<source format="gltf" path="..."/>` (schema allows; loader status unverified)
- `RenderEntity::mesh_type` enum is `0=box, 1=sphere, 2=ground, 3=cylinder`. **There is no OBJ-rendered mesh_type.** An OBJ descriptor can be *parsed* by `mesh_loader::load_obj_geometry`, but the forward renderer does not draw the resulting vertex buffer — it always maps mesh_type to a primitive. Adding an OBJ draw path is a council-triggering renderer change.
- `schemas/mesh.xsd` has an XML-comment-with-double-hyphen authoring bug on line 13 that makes `lxml.etree.XMLSchema(...)` reject the file. For asset validation, parse-only with pugixml shape-match is sufficient; the loader does NOT run XSD.
- `src/core/result.h` uses `std::variant<T,E>` internally — the type-based `std::get<T>` / `std::get<E>` / `variant(T)` idioms are ambiguous when T==E (e.g. `Result<std::string>` where E defaults to std::string). **Fixed 2026-04-21 by switching to index-based `std::get<0>/<1>` and `std::in_place_index<0|1>` in the private constructors.** If you ever touch `result.h`, preserve the index-based form.
- No skinned-mesh loader is wired. `demo/fps_humanoid/` draws a **stick-figure** via `SkeletonRenderer` (cylinders for bones, spheres for joints). The "character" IS the skeleton render. To ship armor, generate extra bone-parented `RenderEntity` primitives — rigid chunks following bone world transforms. Fallback pattern documented in §3d.
- Material format (`schemas/material.xsd`): canonical `<shaders>/<pbr>/<textures>`; legacy short-form `<shader/>` + `<properties>` also valid. Existing demo mats use the legacy form — match it for new demo content.
- Textures must ship as BCn at runtime (BC7 albedo, BC5 normal, BC4 single-channel). No RGBA8 runtime path. Baker is not in-repo yet — omit texture refs until the baker lands.
- Humanoid skeleton is 19 bones at `demo/fps_humanoid/assets/humanoid.skeleton.xml`. Default pose: feet at y=0, root (pelvis) at y=0.95, chest at y=1.35, head-base at y=1.65, total height ≈ 1.8 m. Use this as the armor's scale target.
- Project vibe: **Fantasy Etherealism Impressionism** — matte-black with hard white edge highlights, no chromatic color on armor. Glowing visors = warm-white, not neon.

## 2. Tool availability in this environment (verified 2026-04-21)

- **Blender IS installed at `C:\Program Files\Blender Foundation\Blender 2.93\blender.exe`** (LTS 2.93.5 from 2021-10-06). NOT on PATH — always invoke by full path, quoted.
- Headless invocation: `"/c/Program Files/Blender Foundation/Blender 2.93/blender.exe" --background --python build_script.py` — works fine, emits rich stdout, 2-3 sec for a modest scene.
- `bpy.ops.export_scene.obj(...)` is the **2.93 OBJ exporter**. Flags: `filepath`, `use_triangles=True`, `axis_forward="-Z"`, `axis_up="Y"`, `use_normals=True`, `use_uvs=False`, `use_materials=True`, `use_mesh_modifiers=True` (applies modifiers at export without destroying the authoring stack), `use_selection=True` (select desired meshes first).
- **`bpy.ops.wm.obj_export` is 3.3+ and WILL FAIL on 2.93** — do not use. (Prior agent session authored this call; it silently failed.)
- `bpy.ops.wm.save_as_mainfile(filepath=...)` saves a .blend.
- Running `--python <script>` in 2.93: `__file__` is set in the script's global scope — you can derive its directory via `os.path.dirname(os.path.abspath(__file__))`. `bpy.data.filepath` is empty until the first `save_as_mainfile` succeeds.
- Eevee render from script: set `scn.render.engine = "BLENDER_EEVEE"`, `scn.eevee.taa_render_samples = 64`, `scn.view_settings.view_transform = "Filmic"`, then `bpy.ops.render.render(write_still=True)` with `scn.render.filepath` set. 1920x1080 at 64 TAA samples is ~1 sec on an RTX 3080.
- **No Blender MCP server** — always headless Python scripts, no interactive ops. No Meshy / Rodin / Tripo / Hunyuan / Kaedim MCPs connected.
- Python 3.13 on PATH; lxml 6.0.2 available (useful for XML shape validation).

## 3. Piece-first decomposition — the mandate

Every character-class asset decomposes before any tool is touched. Write the component list, per-piece dimensions, pivot, and intended authoring tactic. Monolithic generation almost always fails on: topology, scale, pivot, attachment alignment, UV seams, and budget.

### 3a. Humanoid heavy-armor template (reusable)

Validated once via the Berserk-Halo Mk3 build: **63 named objects, 10,720 tris total, single Blender headless run, all solidify+bevel+weighted-normal modifiers applied at export**. Use the proportions below as a default for Mjolnir-class or plate-armor humanoids on the 1.8m skeleton. Numbers after the colon are the realized tri count from that build; feel free to spend more budget on silhouette-driver parts (chest, pauldrons, helmet).

| Group | Components | Realized tris |
|-------|------------|---------------|
| Helmet | shell + 2 horns + visor + 2 cheeks | ~780 |
| Torso | sternum + 2 upper + 2 lower chevrons + 3 ribs | ~1950 |
| Back | 7 vertebrae + 2 scapulas + hump | ~1200 |
| Arms L+R | 3 pauldrons + upper_arm + 3 elbow disks + forearm + gauntlet + 5 claws (joined) each side | ~3150 |
| Hip+Legs | belt + codpiece + 2 thighs + 2 thigh_plates + 6 knee disks + 2 shins + 2 greaves + 2 boots + 2 boot_toes | ~3650 |

Total ~10.7k. Hero budget 40k-60k; the faceted-plate style reads brutal without spending more. Push extra budget into scar passes (knife-project panel lines) rather than smoother surfaces.

### 3b. Silhouette-first build order

1. Blockout: helmet → chest slab → pauldron stack → pelvis → legs → boots. Primitive boxes only. No bevels. Eyeball side profile against reference — must read hunched/forward.
2. Horn + visor + shoulder-stack count. These three read from 50 m away. Do not proceed until silhouette is right.
3. Plate pass: solidify each slab, bevel outer edges (0.5-1.0 cm), apply Weighted Normal.
4. Overlap pass: lobster-joint the elbows, knees, hip belt, pauldron stack.
5. Panel/scar pass: knife-project panel lines, add small scar notches via `bmesh.ops.inset_individual(faces=[...], thickness=0.015, depth=-0.007)`.

### 3c. Kit-bash vs AI-generator call

- **Hard-surface plate parts** (chest, pauldrons, greaves, gauntlets): Blender primitives + solidify + bevel beats any AI generator for 2026-era quality. Meshy/Rodin produce blob topology that you'd retopo anyway.
- **Organic details** (horns, claws, scarring): AI generator can give a useful starter shell, but you still retopo before bevel passes. Simple tapered cones via `primitive_cone_add(r1=0.035, r2=0.003, depth=0.30)` rotated are perfectly fine for claws/horns and don't need AI.
- **Sculpted faces/skin**: out of scope here (full-face-covered armor).

### 3d. The bone-parented-primitives runtime fallback

Because OdysseyEngine's forward renderer only draws `mesh_type ∈ {box, sphere, ground, cylinder}`, a baked OBJ can't render directly. Workaround: **build the in-engine armor silhouette out of primitives parented to bone world transforms.**

Pattern, used successfully by `demo/fps_humanoid/berserk_halo_character.{h,cpp}`:

```cpp
struct ArmorPiece {
    std::string bone;        // name in skeleton.bone_index
    vec3 local_offset;       // in bone-local frame
    quat local_rot;
    vec3 scale;              // RenderEntity::scale
    int  mesh_type;          // 0=box, 1=sphere, 3=cylinder
    float shade_darken;      // 0..1 per-piece tint offset
};

// Each frame:
mat4 local = translate(p.local_offset) * toMat4(p.local_rot);
mat4 piece_world = world_transforms_[bone_idx] * local;
entity.position = vec3(piece_world[3]);
entity.rotation = quat_cast(piece_world);
entity.scale    = p.scale;
```

Authoring: derive `local_offset` by subtracting the bone's REST world position from the Blender world-space position you authored the piece at. Bone rest world positions for the 19-bone humanoid are chainable from `humanoid.skeleton.xml` via `sum(parent.position + ... + this.position)` — spine=1.15, chest=1.35, upper_arm_l/r=(±0.15, 1.07, 0), etc.

Gotcha: bone world_transforms already include the root translation applied by the character code (`root_transform = translate(world_pos) * toMat4(world_rot)`), so don't re-apply it in compose_piece.

## 4. Blender 2.93 quirks worth keeping

- **Coordinate convention mismatch**: Blender's viewport/world defaults to +Z up. If you author for engine export with `axis_up="Y"`, the figure stands along +Y in the .blend and will lay on its side when rendered in Blender. For rendering, rotate the root empty 90deg about X temporarily: `root.rotation_euler = (radians(90), 0, 0)`. Restore or just discard — the export already baked the correct orientation.
- **`bmesh.from_edit_mesh` vs `bmesh.new`**: from_edit_mesh must be paired with `bpy.ops.object.mode_set(mode="EDIT")` first and `bmesh.update_edit_mesh(mesh_data)` after. Using `bmesh.new()` is for non-destructive eval; for in-place mesh edits (vertex shifts, face insets), use `from_edit_mesh`.
- **Post-modifier tri counting**: `obj.evaluated_get(depsgraph).to_mesh()` returns the evaluated mesh with all modifiers applied. Triangulated count: `sum(max(1, len(p.vertices) - 2) for p in me.polygons)`. Always `eval_obj.to_mesh_clear()` after reading.
- **`bpy.ops.object.join()` re-uses active object's name** — explicitly rename the joined result and scrub stale entries from your tracking list.
- **TBBmalloc warning on startup**: `"TBBmalloc: skip allocation functions replacement in ucrtbase.dll"` — harmless, ignore.
- **Validation layers absent on this machine**: Vulkan SDK not installed. Engine logs two warnings and continues. Not a modeling concern but shows up in integration-test logs.

## 5. Export conventions for OdysseyEngine

- Units: meters. 1 Blender unit = 1 m.
- Up axis: +Y (engine convention). Forward axis: -Z.
- Pivot: feet-center at origin for full-character rigs; for standalone pieces, pivot at the attachment bone's head position.
- Apply location/rotation/scale before export (or use `use_mesh_modifiers=True` on export to bake the modifier stack without destroying the .blend).
- OBJ export flags for 2.93:
  ```python
  bpy.ops.export_scene.obj(
      filepath=out_path,
      use_selection=True,           # select your meshes first
      use_triangles=True,
      use_normals=True,
      use_uvs=False,                # until BCn baker ships
      use_materials=True,
      axis_forward="-Z",
      axis_up="Y",
      use_mesh_modifiers=True,
  )
  ```
- `.mesh.xml` for an OBJ-backed asset: see `demo/showcase/assets/berserk_halo_mk3/berserk_halo.mesh.xml` for a working example. LOD triangle counts are currently advisory (no runtime LOD switching wired yet).

## 6. Council triggers specific to asset work

Invoke `/council` before committing any of the following (per `CLAUDE.md`):
- New enemy archetype or tone-establishing hero asset (Berserk-Halo Mjolnir qualifies — single-handedly escalatable by Marty + vibe-story-guardian).
- Changes that force a renderer extension (new `mesh_type`, skinned-mesh path, new vertex format).
- Addition of a mesh/material schema element.
- New asset directory structure under `demo/`.

Pure-XML descriptor assets that reference existing renderer primitives or bone-parented primitive composition are **not** renderer changes. Council vote still advisable for tone; not required for architecture.

## 7. Validated pipeline (one-shot)

For a humanoid hero asset, the reliable pipeline is:

1. Read `humanoid.skeleton.xml` — derive chain-summed rest world positions.
2. Write `build_<asset>.py` — one Blender script, builds every component, applies modifiers via export (not pre-export), writes `.blend` + `.obj` + `tri_counts.txt`.
3. Run headless: `"/c/Program Files/Blender Foundation/Blender 2.93/blender.exe" --background --python build_<asset>.py`.
4. Write `<asset>.mesh.xml` pointing at the OBJ. Spot-check vs schema by parsing.
5. Author the animation `.anim.xml` referencing the 19 bone names. Validate monotonic times.
6. Write `<asset>_character.{h,cpp}` in `demo/fps_humanoid/` — uses the bone-parented primitive pattern from §3d. Auto-picked up by `file(GLOB_RECURSE "demo/fps_humanoid/*.cpp")` in CMakeLists.
7. Wire one enemy in `fps_game.cpp` to use the new character variant.
8. Build: `VCToolsVersion=14.42.34433 cmake --build build --config Release --target odyssey_fps`.
9. Smoke-test: `./Release/odyssey_fps.exe run` — watch for `<CharacterName>: initialized with N bones, M armor pieces`.
10. Write `render_<asset>.py` — loads the .blend, rotates root 90deg X for Blender +Z-up, assigns matte + emissive materials, Eevee Filmic/High-Contrast, 64 TAA samples. Headless run for the PNG.

This exact pipeline was validated end-to-end on 2026-04-21 for the Berserk-Halo Mk3 Mjolnir. All 162 unit tests pass; the enemy initializes and renders.

## 8. Gaps to fill on next run

- Skinned-mesh renderer path — council decision. Would let the baked OBJ render directly instead of the primitive decomposition. Gating factor: the full renderer needs a vertex buffer + per-bone matrix palette + a second graphics pipeline.
- BC7/BC5 baker — once this ships, add UV unwrap + normal/AO bake pass to `build_<asset>.py`.
- Action sequence XML (`.actions.xml`) for cinematic moments — heavy taunt, death fall. Schema exists (`schemas/actions.xsd`) but no examples consumed yet.
- Pick one AI generator to trial for organic pieces (claws, broken plates, scar geometry). Meshy Pro API still the lead due to mesh-clean export quality; Rodin second. Still no MCP wired.

---

*Last updated: 2026-04-21 — Berserk-Halo Mk3 Mjolnir end-to-end landed; pipeline validated; §2 corrected (Blender IS installed); §3d added (bone-parented primitives pattern); §1 engine-blocker patch noted.*
