# Phase 9 — Walker Demo Enablement

- Date: 2026-04-22
- Topic tag: physics=true
- Tally: approve=21, reject=0, abstain=0 → 100% (threshold 80%)
- Outcome: RATIFIED

## Proposal

Phase 9 enables a first-person walker demo by (a) extending `EntityComponents` (`src/scene/entity_manager.h`) with a `parent_id` field + `Rigidbody` / `BoxCollider` / `SphereCollider` / `CapsuleCollider` / `MeshCollider` / `CameraComponent` rows; (b) adding corresponding elements and `<entity parent="...">` attribute to `schemas/scene.xsd` and `schemas/prefab.xsd` with serializer round-trip support; (c) composing world transforms as `world = parent.world × local` in `EntityManager`; (d) wiring the existing `PhysicsWorld` into `Engine::process_frame` with a new `Engine::physics_world()` getter; (e) authoring `demo/showcase/walker.scene.xml` with ground + player (CapsuleCollider r=0.4 h=1.8, Rigidbody m=75kg useGravity=true, FirstPersonController script) + child Camera (FOV 70, isMain=true); (f) wiring a directional light + ambient term into `shaders/forward.frag`. Shadow map, terrain, and GLTF loader are explicitly deferred.

## Votes

| Agent | Vote | Weight | Key point |
|---|---|---|---|
| game-ai-engineer | APPROVE | 2 | Generic component names unblock future Nadir archetypes with the same physics rows |
| game-asset-engineer | APPROVE | 2 | XSD discipline is tractable — conditions enforce round-trip invariant |
| game-engine-architect | APPROVE | 3 | Wiring is overdue; determinism + tick order must be explicit |
| lighting-mood-architect | APPROVE | 3 | Directional/ambient must be profile-driven, not hardcoded |
| marty-odonnell-composer | APPROVE | 4 | Walker must ship with Phase 7 music binding + footstep breadcrumb |
| netcode-engineer | APPROVE | 2 | Net-silent now; document deferred replication surface for Phase 11+ |
| vibe-story-guardian | APPROVE | 2 | First-footstep moment sets tone; require mood declaration + named constants |
| 3d-asset-modeler | APPROVE | 2 | Ship with visible placeholder mesh for debug orientation |
| council-implementation-coder | APPROVE | 1 | Mechanically feasible; split into 4 sub-commits (K/J-wire/O/N) |

## Conditions adopted

### Engine/scheduling (architect, weight 3)
- Fixed-dt accumulator: canonical dt=1/60s, max 5 substeps per frame, residual accumulator retained.
- Tick order in `Engine::process_frame` (comment-enforced):
  1. InputManager poll
  2. Script::pre_physics tick (authoritative writes to Rigidbody)
  3. PhysicsWorld::step (fixed-substep loop)
  4. Script::post_physics tick (read-only on physics state)
  5. EntityManager::compose_world_transforms (topological)
  6. NadirSystem dispatch + readback
  7. Renderer::render_frame
  8. AudioMixer::mix
- `Engine::physics_world()` const returns `const PhysicsWorld&`; `physics_world_mut()` gated to pre_physics script phase (ScriptContext phase flag).
- Rigidbody → local_transform writeback is authoritative during physics phase.
- Cycle + self-parent detection: reject at load and at runtime set_parent; depth cap 64; `Result<void, HierarchyError>` with {Cycle, SelfParent, UnknownParent, DepthExceeded}.
- Determinism pipeline test: 600-substep walker fall produces bit-identical final position across two runs.
- Capsule-vs-plane derivation comment required; Box/Mesh collision paths may be stubbed-with-TODO if not exercised by walker.
- Lambertian only; confirm bindless golden-image harness stays ΔE<1.0 after forward.frag change.

### Asset/schema (asset-engineer, weight 2)
- C1: `<entity parent="...">` optional in scene.xsd AND prefab.xsd. Never emit when absent.
- C2: Component elements (Rigidbody/BoxCollider/SphereCollider/CapsuleCollider/MeshCollider/CameraComponent) defined once via `xs:group` or `xs:complexType` referenced by both schemas.
- C3: Serializer edits original pugixml doc in place, never emits schema defaults. Byte-identical round-trip test for walker.scene.xml + 3 existing parent-less scenes.
- C4: Pure `Result<T,E>` parser per component element; success + failure tests per error mode (malformed vec3, negative radius, capsule h<2r, mass<=0, missing required attr).

### AI (ai-engineer, weight 2)
- `parent_id` stored as a FIELD on the transform row — not a new component type. Must not fragment archetype grouping or shift SSBO indices Nadir dispatches.
- `Rigidbody`/`Collider` rows addable to any archetype without C++ change (schema-driven attach).
- Expose `is_grounded` + `velocity` to spatial/physics SSBO for future Nadir behaviors.
- `kinematic` flag on Rigidbody for AI-driven entities that want physics collision without dynamic response.

### Lighting (lighting-mood, weight 3)
- Directional light direction/color (Kelvin-derived)/intensity + ambient term sourced from active `LightingProfile`. No hardcoded shader constants.
- Profile-less fallback: sun=normalize(-1,-2,-1), 6500K, intensity 1.0, ambient 0.15 neutral grey. Commented as "debug fallback".
- walker.scene.xml MUST bind a lighting_profile. Recommend new `walker_testbed` profile (5600K sun + 5000K ambient, low contrast, vignette 0.15, bloom 1.1@0.3).
- Post-FX stack order unchanged.
- ΔE<1.0 regression check on all 6 profiles against existing bindless golden-image harness.
- One-sentence derivation comment per lighting term in forward.frag (M3).

### Composer (marty, weight 4)
- walker.scene.xml binds lighting_profile_ref that resolves to one of the 6 Phase 7 mood themes via the scene_theme chain.
- FirstPersonController source includes `// AUDIO-HOOK (Phase 10+):` comment at ground-contact / landed-this-frame site naming the future `footstep_event` payload.
- Any Phase 7 regression in the scene_theme chain escalates to Marty before merge.
- Document that footstep SFX + ambient beds + reverb-zones are deferred to a new SfxDirector subsystem (future council vote).

### Net (netcode, weight 2)
- Decision record adds "Deferred Net Surface" section enumerating per-component replication intent: Rigidbody=server-auth-Phase-11+, Collider=config-only, parent_id=server-auth-reliable, CameraComponent=client-local ALWAYS.
- EntityComponents field additions use stable-ordinal keys — explicit enum values in `src/scene/component_ids.h` (new file). No alphabetical / insertion-order dependencies.
- CameraComponent carries `constexpr bool kReplicated = false` marker.
- FirstPersonController's input capture + camera pose are client-side prediction territory; camera never server-reconciled.

### Vibe (story-guardian, weight 2)
- walker.scene.xml ships with stated-mood top comment citing Vibe Charter.
- FirstPersonController default values (walk_speed, jump_impulse, gravity, eye_height, optional head_bob) declared as named constants with one-line feel comments. No magic numbers.
- If Vibe Charter has no locomotion pillar yet, add draft pillar in same PR: *"movement feels weighted and deliberate; the player is a body in a world, not a camera on rails"*.
- Lighting binding via profile name, not inline RGB literals.

### 3D assets (3d-asset-modeler, weight 2)
- Player entity ships with visible placeholder mesh (cylinder or sphere stack sized to CapsuleCollider r=0.4 h=1.8, pivot at feet).
- Document player-entity convention: pivot at feet, +Y up, 1 unit = 1 meter, forward = -Z.
- parent_id missing/cycle behavior pinned in XSD documentation.

### Coder feasibility (weight 1)
- Split into 4 sub-commits: K (parenting) + J-wire (physics) + O (camera) + N (walker scene + lighting).
- Serializer round-trip invariant: empty parent absent → no attribute emitted.
- All Result<T,E> returns get success + failure tests.

## Deferred Net Surface (per netcode condition)

| Component | Replication intent | Phase |
|---|---|---|
| Rigidbody.position/velocity/angular_velocity | Server-authoritative, quantized | 11+ |
| Rigidbody.mass/drag/useGravity/isKinematic | Config-only, never replicated | — |
| BoxCollider/SphereCollider/CapsuleCollider/MeshCollider | Config-only, never replicated | — |
| parent_id | Server-authoritative entity-graph edge, reliable channel | 11+ |
| CameraComponent (all fields) | Client-local ALWAYS, never crosses wire | — |
| FirstPersonController input | Client-side prediction + server reconciliation for rigidbody; camera never reconciled | 11+ |

## Additions adopted

- Capsule-vs-capsule broadphase hook stubbed now (cheap insurance for enemy AI).
- Transform composition caches sort order; only re-sort on parent_id change.
- Smoke test: walker.scene.xml loads → MusicDirector enters expected state (suggest, not require).

## Dissent recorded

None. 21/21 APPROVE, 100% consensus.
