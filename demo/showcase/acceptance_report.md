# Showcase acceptance report

Per-agent checklists, filled in during each acceptance run.

## game-engine-architect (Rendering / Physics / Editor)
- [ ] /mode-switch cycles Play→Edit→Simulate without crash
- [ ] /barrier-audit shows explicit offscreen semaphore
- [ ] /descriptor-dump shows bindless slots populated
- [ ] /physics-step deterministically steps one frame
- [ ] /integrator-test matches closed-form analytic solution within 1e-6
- [ ] /rigidbody-add attaches without scene XML corruption
- [ ] PatrolScript attaches at runtime and ticks
- [ ] Crate prefab round-trips with rigidbody block preserved

## vibe-story-guardian (Vibe / Story / Felt Experience)
- [ ] /anti-touchstone-check PASS on every file under demo/showcase/ (materials, meshes, prefabs, lighting, post-FX, music, behaviors, scripts)
- [ ] /charter-check BLESSED for each of the 7 pillars against showcase evidence (see design/vibe_acceptance.md section 2)
- [ ] Forbidden-list scan — zero hits across the eight items (damage numbers, waypoints, tutorial prose creep, etc.)
- [ ] Sacred: player death is unglamorized, unrewarded, and scored with the single low tone
- [ ] Sacred: 30 s standstill at scene open produces no music escalation — silence holds
- [ ] Sacred: /inspect-scoring confirms Nadir parallel dispatch, no CPU serial fallback
- [ ] Atmospheric arc reads in order: Arrival (15 s bed only) → Recognition (first hostile LOS fires stinger + combat crossfade) → Aftermath (victory tail OR single low fail tone)
- [ ] /vibe-audit demo/showcase/ returns zero HIGH-severity drifts
- [ ] Walkthrough: a first-time player can articulate "the world was already running" after one playthrough

## game-ai-engineer (Behaviors / Nadir)

See `design/ai_acceptance.md` for the full scoring and replay gates.

| criterion                                                              | status | notes |
|------------------------------------------------------------------------|--------|-------|
| All 5 showcase-referenced shaders compile in odyssey_tests_pipeline    | [ ]    | NadirShowcaseCompile.ShowcaseReferencedShadersCompile |
| Every .nadir in demo/behaviors + demo/showcase/behaviors compiles      | [ ]    | NadirShowcaseCompile.AllShadersInBothBehaviorDirsCompile |
| /inspect-scoring: player_input is pass-through from persist.memory_0   | [ ]    | move_vector matches CPU input; action_request == 0 |
| /inspect-scoring: pack hunter PATROL→ALERT→COMBAT transitions on range | [ ]    | 30/20/10 u gates; comms_signal on COMBAT entry |
| /inspect-scoring: sniper holds 25-40 u band, fires on bell_curve peak  | [ ]    | cooldown_0 gates attack_target.w |
| /inspect-scoring: boss 4-arm scores visible via debug_pack4            | [ ]    | shotgun/sniper/smg/shield; overload on 3+ > 0.4 |
| /inspect-scoring: civilians flee inside 25 u, scream once on entry     | [ ]    | STATE_FLEE gated by score_proximity > 0.3 |
| /replay-step: no state oscillates >1x/sec (hysteresis holds)           | [ ]    | hysteresis_bonus values tuned per archetype |
| /replay-step: boss COVER only when taking damage AND low HP            | [ ]    | exits within 2 s of damage ceasing |
| /replay-step: pack cohesion — 6/8 hunters within 15 u of ally_center   | [ ]    | over 300-tick window |
| No move_vector NaN, no length > speed*1.1 on any entity                | [ ]    | steering clamp in every shader |
| Boss overload (action_request=4) fires at most once per 6 s            | [ ]    | action-cooldown guard |

## lighting-mood-architect (Lighting / Mood / Post-FX)

Full criteria in `design/lighting_acceptance.md`. Stub rows below for the run tally.

| Criterion | Status | Notes |
|---|---|---|
| A. Kelvin palette adherence (all 14 lights)                       | [ ] | `/kelvin-preview` per light; cross-check against zone profile palette bands. |
| B. Light count budget (≤ 8 per zone, 14 total)                    | [ ] | `/descriptor-dump` per zone; South Pit at cap (8) — do not regress. |
| C. Flicker determinism (bit-identical on repeat + replay)         | [ ] | `torch_north` is flagship; hash LightBuffer rows across two runs. |
| D. Fog-per-zone correctness (density, color, volumetric on/off)   | [ ] | Liminal + Hostile must report volumetric pass skipped. |
| E. Bindless descriptor validation (LightBuffer + grade LUT + fog) | [ ] | `/descriptor-dump`; no `VK_NULL_HANDLE` in populated range. |
| F. Post-FX barrier audit (explicit order, no implicit edges)      | [ ] | `/barrier-audit --strict`; CRT + EVA stay engine-owned at tail. |
| G. Vibe / charter check (no UE5-default, no PBR cargo-cult)       | [ ] | `/vibe-audit lighting_profiles/`; `/anti-touchstone-check` per profile. |
| H. Regression safeguards (roundtrip, validate-asset, frame-time)  | [ ] | XSD pending (see `design/lighting.md` §8); frame-time budgets per zone in checklist §H. |

## game-asset-engineer (Meshes / Materials / Round-trip)
Audit run on 2026-04-20. Asset coverage for `demo/showcase/showcase.scene.xml`.

### Referenced assets (from scene)
- Meshes: `arena_floor.mesh.xml`, `pillar.mesh.xml`, `crate.mesh.xml`
- Materials: `gold_leaf.mat.xml`, `painted_wood.mat.xml`; `stone_wall.mat.xml`
  exists but is not yet wired via `material_override=` on any entity (reserved
  for walls once the loader preserves that attribute — see gap below).

### XSD validation (lxml against schemas/*.xsd)
- [x] PASS `demo/showcase/meshes/arena_floor.mesh.xml` — mesh.xsd
- [x] PASS `demo/showcase/meshes/pillar.mesh.xml` — mesh.xsd
- [x] PASS `demo/showcase/meshes/crate.mesh.xml` — mesh.xsd
- [x] PASS `demo/showcase/materials/gold_leaf.mat.xml` — material.xsd
- [x] PASS `demo/showcase/materials/painted_wood.mat.xml` — material.xsd
- [x] PASS `demo/showcase/materials/stone_wall.mat.xml` — material.xsd
- [x] EXPECTED-FAIL `demo/showcase/showcase.scene.xml` — scene.xsd rejects
  `scene@lighting_profile`, `scene@audio_bank`, `<world>` child elements,
  `entity@material_override`, and `<entity>` children of `<spawn_region>`
  that carry lights/audio. This is the intentional "preserve unknowns" surface
  flagged by the scene's comment block.

### Engine parse + test suite
- [x] `odyssey_tests_unit.exe` — 118/118 PASS (no regression).
- [x] Engine `scene_loader::parse_scene_xml` accepts the file.

### Round-trip gap (flagged — do NOT fix here)
- [ ] Byte-identical round-trip of `showcase.scene.xml` via the engine.
  - No `scene_serializer` exists yet; `src/scene/` only contains
    `scene_loader.{h,cpp}`. There is no write-back path.
  - `SceneData::EntityDesc` drops every attribute the schema does not know
    about: `material_override` on `<entity>` is never read
    (`scene_loader.cpp:118-128` only reads `<mesh>` and `<material>` children);
    light and audio attributes (`light_type`, `kelvin`, `intensity`, `range`,
    `flicker_amp`, `flicker_hz`, `direction`, `cone_inner`, `cone_outer`,
    `music_state_machine`, `initial_state`, `bus`, `loop`, `src`, `volume`)
    have no fields on the struct.
  - Delivering the promised path requires (per the new-asset-type ritual in
    agent memory): extend `SceneData::EntityDesc` with a preserved-attribute
    map (or retain the pugi::xml_document and edit in place), add
    `src/scene/scene_serializer.{h,cpp}`, add a `tests/unit/` byte-diff test,
    and decide whether `scene.xsd` grows to cover `light_type`/audio/etc. or
    stays intentionally narrow with the serializer honoring unknowns.
- Mesh + material XMLs are lossless under their canonical-form loaders, so
  nothing on the asset side blocks a future scene serializer.
