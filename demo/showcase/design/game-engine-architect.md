# Showcase — Rendering / Physics / Editor contribution

**Agent:** game-engine-architect
**Scope:** rendering pipeline, first-principles rigid body (NOT Jolt), editor modes,
bindless descriptors, offscreen↔ImGui sync, hot-reload barriers.
**Root:** `demo/showcase/`

## 1. Rendering coverage

| Feature                         | Exercised by                                                                                     | Notes                                                                                                                                                 |
| ------------------------------- | ------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------- |
| Primitive mesh archetypes       | showcase scene uses box (crate), sphere (projectile), ground plane, cylinder (skeleton bones)    | each `RenderEntity::mesh_type` (0-3) is represented at least once so a regression in the primitive cache is visible without importing any asset      |
| Imported mesh + material        | humanoid enemies via `humanoid.skeleton.xml` + `walk_cycle.anim.xml`                             | proves the `.mesh.xml` → mesh_loader path                                                                                                            |
| Bindless material/texture       | 4+ distinct materials live: `crate.mat.xml`, `player.mat.xml`, `projectile.mat.xml`, `enemy_dark.mat.xml` | `/descriptor-dump` shows populated slots; shader-side indices must match CPU-side                                                                    |
| Kelvin-temperatured lights      | wired through `lighting_profiles/Liminal.xml` once the lighting agent ships the LightBuffer SSBO | until then the renderer uses its flat ambient; the design is that `descriptor-dump` will surface the LightBuffer as one more bindless slot           |
| Offscreen → post → ImGui path   | CRT + EVA HUD + (lighting-mood fog) → editor viewport ImTextureID                                | validated by `/barrier-audit` — expect explicit semaphores, explicit layout transitions, no implicit aliasing                                        |

### Bindless discipline

The bindless table is a single `VK_EXT_descriptor_indexing` descriptor set with
`VARIABLE_DESCRIPTOR_COUNT` + `UPDATE_AFTER_BIND` + `PARTIALLY_BOUND` flags.
Slots are allocated monotonically per material load; free slots become a reuse
free-list (no compaction during play). `/descriptor-dump` must show:

- sampled-image array: at least one slot per loaded texture
- SSBO: LightBuffer, NadirEntity buffers, BehaviorOutput
- sampler array: point, linear, anisotropic (3 entries minimum)

A failure mode to catch: a shader that reads slot N while the CPU believes N
is unbound. `descriptor-dump` diffs the two sources of truth.

## 2. Physics coverage

The showcase is the canonical stress for the first-principles rigid body.
**Jolt is explicitly not used.** We ship a pure semi-implicit Euler integrator
against a sequential-impulse contact solver; gravity is a uniform, everything
else is a force accumulator.

| Scenario                  | Exercised by                                                                                              |
| ------------------------- | --------------------------------------------------------------------------------------------------------- |
| Player-ground             | showcase player (capsule) vs. the demo's `CollisionSystem` ground plane                                   |
| Projectile-enemy          | projectile prefab sphere vs. enemy capsule; sphere_vs_capsule narrowphase                                 |
| Crate stack (phase 4)     | 3×3×3 stack of `crate.prefab.xml` under gravity; settles within N steps with restitution 0.2, friction 0.55 |
| Integrator analytic check | `/integrator-test` under constant gravity — closed-form `x(t) = x0 + v0 t + ½ g t²` within 1e-6           |
| Deterministic stepping    | `/physics-step` runs one tick, dumps pre/post state — same inputs must produce byte-identical outputs     |

### Why first-principles, not Jolt

1. **Vibe physics over reality physics.** We want hit-stop, coyote time, and
   squash-and-stretch to be first-class. A bespoke integrator lets us override
   gravity per-entity (floaty jumps), freeze position on hit-stop frames, and
   damp angular velocity asymmetrically — all without fighting a black-box
   simulator.
2. **Pure-function grain.** The engine's functions are pure; Jolt's API is
   mutation-heavy. A first-principles solver fits the `integrate() →
   broadphase() → narrowphase() → solve() → apply()` stage-pipeline naturally,
   each stage returning a struct.
3. **Determinism.** We need byte-stable physics for the netcode agent's replay
   determinism story. Our own integrator gives us that; Jolt would force us to
   pin a specific build + runtime config.
4. **GPU-maximalist future.** The crate-stack solver is a stepping stone to a
   compute-shader solver that dispatches alongside Nadir — exactly the
   OdysseyEngine idiom.

## 3. Editor mode validation

`Engine::set_mode(Play | Edit | Simulate)` is the single entry point.
Invariants, validated by the `/mode-switch` skill:

| Mode       | dt     | Nadir dispatch | Scripts tick | Physics step | Input to game | Editor chrome |
| ---------- | ------ | -------------- | ------------ | ------------ | ------------- | ------------- |
| `Play`     | real   | yes            | yes          | yes          | yes           | **hidden**    |
| `Edit`     | frozen (0) | **skipped**  | skipped      | skipped      | editor only   | visible       |
| `Simulate` | real   | yes            | yes          | yes          | **editor only** | visible    |

Acceptance: `/mode-switch` must cycle `Play → Edit → Simulate → Play` without a
crash, and each transition must satisfy its column above — e.g. in Edit the
Nadir dispatch counter doesn't advance between frames, the script runner's
frame_number is pinned, and rendering still composes a frame (so you can see
the scene you're editing).

## 4. Offscreen → ImGui sync

The scene renders into an offscreen R16G16B16A16_SFLOAT image
(`PostProcessor::begin_frame_offscreen`), walks the post-FX chain, then is
sampled by ImGui as a texture for the Viewport panel. The sync contract:

- offscreen render finishes → **signal** semaphore `offscreen_complete`
- post-process composition → **waits** on `offscreen_complete`
- ImGui pass samples the final image → **waits** on `postprocess_complete`
- ImGui submission → **signals** the swapchain-present semaphore

`/barrier-audit` must show every edge in that graph explicitly. The failure
we're hunting is implicit aliasing — a driver that happens to serialise those
ops today but will not on a different GPU. The image layout transition
`COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL` must appear in the
barrier list, not be inferred from a render-pass dependency.

## 5. Hot-reload barrier acceptance

Editing `showcase.scene.xml` from the Hierarchy/Inspector or from disk must:

1. Engine enters a GPU **drain** — wait on the last-submitted fence before
   reloading.
2. The scene load builds a fresh `EntityComponents` snapshot; the generation
   counter bumps (`scene_generation++`).
3. `/barrier-audit` run *during* the reload window shows no GPU hazards —
   every in-flight resource has been released, no descriptor set is mid-use.
4. Any dangling handle from before the reload is invalidated by the
   generation bump and safely no-ops.

The `Script Console → attach PatrolScript to <id>` path uses the same
generation gate: the script appears on the next tick after the bump, never
mid-frame.

## 6. Authoring notes — prefab shape

`prefab.xsd` currently uses `xs:all` with `maxOccurs="1"` per element. Two
implications for this contribution:

- **RigidBody** has no schema entry yet. Both `crate.prefab.xml` and
  `player.prefab.xml` carry the intended rigidbody block in a top-of-file
  authoring comment; phase-4's `/schema-add` adds the `<rigidbody>` XSD node
  and the editor's `/rigidbody-add` is expected to un-stub those blocks.
- **Multiple `<script>`s** per prefab are not expressible yet (xs:all
  limitation). `player.prefab.xml` here carries `RespawnScript` only;
  `PlayerController` attachment for the showcase is done at runtime via
  the script runner from `showcase_player.cpp`, mirroring the demo path.
  Phase-4 schema extension should promote `<script>` to an unbounded
  `<xs:sequence>` so both attach at load time from XML.

## 7. Pass / fail signals

**Pass:**
- `/mode-switch` cycles cleanly, state invariants hold per the table above.
- `/barrier-audit` shows the full explicit graph with no "implicit" edges.
- `/descriptor-dump` slots populated, shader-read indices match CPU-bound.
- `/physics-step` twice with same input → byte-identical output.
- `/integrator-test --force=gravity --dt=0.016 --steps=600` → max error 1e-6.
- `/rigidbody-add crate 25 0.55 0.20` round-trips through the serializer.
- `PatrolScript` attached from the Script Console steers the scout on the
  next tick.

**Fail (examples to guard against):**
- Editing the scene mid-frame corrupts a descriptor index → `/descriptor-dump`
  reports a slot referenced by a shader but bound to `VK_NULL_HANDLE`.
- `/barrier-audit` reports a layout transition without a preceding
  pipeline-barrier — driver-dependent hazard.
- `/integrator-test` drift > 1e-6 → integrator is accumulating error; suspect
  an Euler regression or a force-accumulator that was not zeroed per step.
- Entering Edit mode still ticks the Nadir dispatch counter — mode gating
  bypassed somewhere in the main loop.
