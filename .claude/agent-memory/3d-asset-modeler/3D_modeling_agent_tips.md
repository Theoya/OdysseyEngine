# 3D Modeling Agent — Persistent Tips

Living playbook for AI-agent-driven 3D asset production in OdysseyEngine.
Keep this doc lean. Trim, reorganize, and deduplicate as tactics land or go stale.

---

## 1. Host-project ground truth (OdysseyEngine, checked 2026-04-21)

- Mesh format is a **descriptor-only XML** (`schemas/mesh.xsd`). It points at one of:
  - `<source format="primitive">box|sphere|cylinder|capsule|ground</source>`
  - `<source format="obj"  path="demo/.../foo.obj"/>`
  - `<source format="gltf" path="..."/>` (schema allows it; loader status unverified)
- No skinned-mesh loader is wired. `demo/fps_humanoid/` draws a **stick-figure**: cylinders for bones, spheres for joints, via the `SkeletonRenderer`. There is no humanoid body mesh asset in the repo — the character IS the skeleton render.
- `RenderEntity::mesh_type` enum is `0=box, 1=sphere, 2=ground, 3=cylinder`. Adding a new runtime mesh type requires touching the renderer (`src/vulkan/renderer*`) — this is a council-triggering change.
- Material format (`schemas/material.xsd`): canonical form uses `<shaders>`/`<pbr>`/`<textures>` blocks; legacy short form uses `<shader/>` + `<properties>`. Both validate. Existing demo mats use the legacy short form — match that for new demo content unless migrating wholesale.
- Textures must ship as BCn at runtime (BC7 albedo, BC5 normal, BC4 single-channel). No RGBA8 runtime path. Baker is not in-repo yet — leave texture refs out of a material until the baker exists.
- Humanoid skeleton is 19 bones at `demo/fps_humanoid/assets/humanoid.skeleton.xml`. Default pose: feet at y=0, root at y=0.95, total height ≈ 1.8 m, arms down. Use this as the armor's scale target.
- Project vibe: **Fantasy Etherealism Impressionism** — hard white edge highlights, deep blacks, no chromatic color on armor-tier assets. Matte-black with edge light, not chrome.

## 2. What this environment actually provides (important reality check)

Before accepting an asset task, confirm which of these are available. As of 2026-04-21 the shell I run in has **none** of the following:

- No Blender on PATH. `which blender` fails. Default `C:/Program Files/Blender Foundation/` directory absent.
- No Blender MCP server connected. No `mcp__blender__*` tools in the tool registry.
- No Meshy / Rodin / Tripo / Hunyuan3D / Luma Genie / Kaedim MCPs connected.
- No Instant Meshes / ZRemesh / decimate CLI.
- No Substance / Dream Textures / Material-X baker.

**What I do have:**

- File I/O, Grep, Glob, Web search/fetch (via ToolSearch).
- OdysseyEngine skills: `create-mesh`, `create-material`, `validate-asset`, `thumbnail-bake` (deferred-impl per its own doc), `council`.
- Python 3.13 on PATH (useful for generating simple procedural OBJs via numpy — no GUI).

**Consequence:** "Build a hero character model" tasks cannot be completed end-to-end in this environment. What can be done:
1. Decompose the asset and produce a build plan the user can execute in their local Blender.
2. Scaffold OdysseyEngine-side files: `.mat.xml` (palette), `.mesh.xml` (stubbed to a placeholder primitive or to a future `.obj` path).
3. Procedurally author simple OBJs (hard-surface primitives, extruded plates) via a Python script if truly needed.
4. Escalate with a council invocation when the asset class (hero character, tone-establishing) demands it.

Do NOT fabricate `.obj`/`.blend`/`.fbx` output or claim a render was baked. State the blocker and give the user a runnable handoff.

## 3. Piece-first decomposition — the mandate

Every character-class asset decomposes before any tool is touched. Write the component list, per-piece dimensions, pivot, and intended authoring tactic. Monolithic generation almost always fails on: topology, scale, pivot, attachment alignment, UV seams, and budget.

### 3a. Humanoid heavy-armor template (reusable)

Use this as the default decomposition for any Mjolnir-class or plate-armor humanoid. Dimensions assume the 1.8 m humanoid skeleton above.

| Component              | Bone binding         | Approx dims (m)      | Pivot        | Tri budget | Notes |
|------------------------|----------------------|----------------------|--------------|------------|-------|
| Helmet shell           | head                 | 0.26 w × 0.32 h × 0.30 d | skull base   | 2500       | hard-surface; keep visor slit as a narrow emissive quad |
| Helmet horns (x2)      | head                 | 0.05 × 0.22 × 0.18   | horn root    | 600×2      | array + curve for taper; shade-flat on edges |
| Jaw/cheek plates       | head                 | 0.10 × 0.08 × 0.12   | cheek hinge  | 400        | two mirrored pieces |
| Chest carapace         | chest                | 0.50 × 0.40 × 0.25   | sternum      | 4000       | layered — build top→bottom, solidify each plate |
| Torso rib segments     | chest→spine          | 0.36 × 0.12 × 0.22 ea| segment mid  | 400 × 5    | array modifier along spine curve |
| Back spine vertebrae   | spine                | 0.08 × 0.06 × 0.10 ea| segment mid  | 300 × 7    | array + curve — mirror of torso ribs concept |
| Upper-back hump        | chest (rear)         | 0.36 × 0.20 × 0.18   | hump base    | 1500       | optional jetpack read |
| Scapula trapezoid (x2) | shoulder_l/r         | 0.22 × 0.24 × 0.06   | medial edge  | 500×2      | flat trapezoid, bevel the outer corners |
| Pauldron stack (x2)    | shoulder_l/r         | 0.24 × 0.26 × 0.22   | shoulder pivot | 1800×2   | 3–4 plates, each a dragon-scale slice; keep inner verts shared |
| Upper arm plate (x2)   | upper_arm_l/r        | 0.14 × 0.28 × 0.14   | arm joint    | 500×2      | cylindrical with panel cuts |
| Elbow lobster (x2)     | upper_arm→lower_arm  | 0.12 × 0.06 × 0.12   | elbow axis   | 300×2 × 3 plates | overlap so elbow bend never reveals gap |
| Forearm plate (x2)     | lower_arm_l/r        | 0.12 × 0.25 × 0.12   | forearm mid  | 600×2      | tapered; outer-edge bevel |
| Gauntlet (x2)          | hand_l/r             | 0.14 × 0.15 × 0.12   | wrist        | 1000×2     | boxy, hide knuckle mech |
| Claws (x10)            | hand_l/r             | 0.02 × 0.05 × 0.015  | knuckle      | 60×10      | quick swept-cone; shade-flat |
| Hip belt               | root                 | 0.38 × 0.12 × 0.30   | pelvis       | 1200       | multi-plate, overlapping |
| Codpiece / groin plate | root                 | 0.18 × 0.18 × 0.10   | front of pelvis | 400     | triangular downward taper |
| Upper leg plate (x2)   | upper_leg_l/r        | 0.16 × 0.40 × 0.16   | thigh mid    | 800×2      | split front/rear plates, overlap side |
| Knee lobster (x2)      | upper→lower_leg      | 0.14 × 0.08 × 0.14   | knee axis    | 300×2 × 3 plates | as elbow |
| Lower leg plate (x2)   | lower_leg_l/r        | 0.14 × 0.40 × 0.14   | shin mid     | 800×2      | greave + calf piece |
| Boot (x2)              | foot_l/r             | 0.16 × 0.12 × 0.26   | heel bottom  | 1000×2     | hooved/wedge silhouette; pivot at ground contact |

Approximate totals: **~34k tris**. Hero budget 40k–80k tris leaves 6k–46k headroom for scars, panel-line detail, and edge bevels. Keep the spine array cheap; push budget into chest carapace + pauldrons (silhouette drivers).

### 3b. Silhouette-first build order

1. Blockout: helmet → chest slab → pauldron stack → pelvis → legs → boots. Primitive boxes only. No bevels. Eyeball side profile against reference — must read hunched/forward.
2. Horn + visor + shoulder-stack count. These three read from 50 m away. Do not proceed until silhouette is right.
3. Plate pass: solidify each slab, bevel outer edges (0.5–1.0 cm), apply Weighted Normal.
4. Overlap pass: lobster-joint the elbows, knees, hip belt, pauldron stack. Ensure joints still hide on bend.
5. Panel/scar pass: knife-project panel lines, add small scar notches. Keep under 10% of budget.

### 3c. Kit-bash vs AI-generator call

- **Hard-surface plate parts** (chest, pauldrons, greaves, gauntlets): Blender primitives + solidify + bevel beats any AI generator for 2026-era quality. Meshy/Rodin produce blob topology that you'd retopo anyway.
- **Organic details** (horns, claws, scarring): AI generator can give a useful starter shell, but you still retopo with Instant Meshes or Quad Remesher before bevel passes.
- **Sculpted faces/skin**: out of scope here (this is full-face-covered armor).

## 4. Tool prompts worth keeping (stubs — fill in when used)

*(empty; add after first successful generator run)*

## 5. Export conventions for OdysseyEngine

- Units: meters. 1 Blender unit = 1 m.
- Up axis: +Y. Forward axis: +Z (loader-agnostic; verify when OBJ loader is exercised).
- Pivot: feet-center at origin for full-character rigs; for standalone pieces, pivot at the attachment bone's head position.
- Apply location/rotation/scale before export.
- OBJ export flags: triangulate, include normals, include UVs, one object per file unless the engine combines (it currently does not have a combining loader).
- `.mesh.xml` for an OBJ-backed asset:
  ```xml
  <mesh name="foo" version="1">
    <source format="obj" path="demo/.../foo.obj"/>
    <lod>
      <level distance="0"  triangles="N"/>
      <level distance="30" triangles="N/4"/>
    </lod>
    <collider type="capsule" radius="0.5" height="2.0"/>
  </mesh>
  ```
- Until the OBJ pipeline is battle-tested, use a primitive-backed stub as a placeholder in the scene, and keep the OBJ next to the `.mesh.xml` so a single-line edit flips it on.

## 6. Council triggers specific to asset work

Invoke `/council` before committing any of the following (per `CLAUDE.md`):
- New enemy archetype or tone-establishing hero asset (Berserk-Halo Mjolnir qualifies — single-handedly escalatable by Marty + vibe-story-guardian).
- New asset directory structure under `demo/`.
- Any change that forces a renderer extension (new `mesh_type`, skinned-mesh path, new vertex format).
- Addition of a mesh/material schema element.

If the asset ships as pure-XML descriptors referencing existing renderer primitives or an OBJ through the existing `obj` source path, that is NOT a renderer change — council vote still advisable for tone, not required for architecture.

## 7. Gaps to fill on next run

- Validate that the `obj` source path in `mesh_loader.cpp` actually parses UVs/normals round-trip. Write a one-triangle OBJ and run pipeline tests before trusting it for a hero asset.
- Stand up a Blender MCP or add Blender to PATH on this dev machine so agents can script modeling operations.
- Decide whether a skinned-mesh path gets added to the renderer (council decision). Until then, armor is static-posed only and cannot follow the existing walk_cycle animation.
- Pick one AI generator to trial for organic pieces (horns/claws). Meshy Pro API is the current lead due to mesh-clean export quality; Rodin is second.

---

*Last updated: 2026-04-21 — initial creation in response to Berserk-Halo Mk3 Mjolnir task.*
