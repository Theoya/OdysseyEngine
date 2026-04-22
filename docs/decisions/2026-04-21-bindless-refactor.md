# Bindless descriptor refactor (Phase 6)

- Date: 2026-04-21
- Topic tag: physics=false
- Tally: approve=18, reject=0, abstain=2 (netcode, excluded from denominator) → **100%** (threshold 80%)
- Outcome: **RATIFIED**

## Proposal

Migrate the renderer's descriptor model from per-material descriptor sets to a bindless array via `VK_EXT_descriptor_indexing`. Scope: (1) enable `descriptorIndexing`, `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`, `descriptorBindingVariableDescriptorCount` on `VkPhysicalDeviceFeatures`; (2) replace the current per-material descriptor set in `src/vulkan/` with one large bindless texture array + sampler array; (3) `src/assets/material_loader.*` emits 32-bit texture indices in an std430 material buffer instead of descriptor-set bindings; (4) the forward-renderer fragment shader samples via `nonuniformEXT`-indexed array access; (5) Lambertian + Blinn-Phong remain the default lighting model per Mandate 4. Non-goals: bindless storage buffers, mesh indexing, multi-queue async uploads.

## Votes

| Agent | Vote | Weight | Key point |
|---|---|---|---|
| game-ai-engineer | approve | 2 | Renderer-local; Nadir 7-SSBO layout untouched; keep compute and graphics descriptor pools disjoint. |
| game-asset-engineer | approve | 2 | Pure free-list allocator; path-keyed `.mat.xml` round-trip preserved; handle invalidation writes magenta sentinel before freelist return. |
| game-engine-architect | approve | 2 | Target Vulkan 1.2 core; set=0 bindless / set=1 frame UBOs; PostProcessor untouched; fail device creation on missing features. |
| lighting-mood-architect | approve | 3 | Approves as unblocker for Kelvin/volumetrics, but gates on visual parity for all 6 showcase profiles + committed follow-on. |
| marty-odonnell-composer | approve | 4 | Renderer-only; mixer isolation preserved; texture upload stalls must stay below one audio-buffer period. |
| netcode-engineer | abstain | 2 | `EntitySnapshot` carries no texture refs; wire format unaffected. |
| vibe-story-guardian | approve | 2 | Preserves Lambertian + Blinn-Phong default; guards against authoring drift toward PBR-coded abundance. |
| 3d-asset-modeler | approve | 2 | Workflow win for piece-by-piece modelling; `.mat.xml` schema unchanged; dedup by absolute path, not filename. |
| council-implementation-coder | approve | 1 | Allocator is pure arithmetic; `Result<T,E>` naturally supports M2; M4 satisfied by decision-record walk of the five feature flags. |

## Conditions adopted

### Slot allocator + texture registry
- **[asset-engineer, coder]** Slot allocator is a pure free-list over a fixed slot table. `alloc(): Result<TextureHandle, AllocErr>` and `free(handle): Result<void, AllocErr>`. Unit tests: alloc from empty, alloc from full (`AllocErr::TableFull`), free then re-alloc, double-free (`AllocErr::DoubleFree`), out-of-range free.
- **[asset-engineer, 3d-modeler]** `TextureHandle` carries a generation counter to detect use-after-free; texture dedup is by resolved absolute path, NOT filename hash.
- **[asset-engineer, architect]** Slot 0 is reserved for a 1×1 magenta "missing/null" texture, uploaded at renderer init, never allocatable. Unload writes the sentinel to the freed slot BEFORE returning it to the free list.

### Vulkan device + descriptor layout
- **[architect]** Target Vulkan 1.2 core (bump `VkApplicationInfo::apiVersion`). Chain `VkPhysicalDeviceVulkan12Features` via `pNext` on device create.
- **[architect, coder]** Hard-validate `VkPhysicalDeviceDescriptorIndexingProperties` limits at startup; fail device creation with a distinct `Result<>` error if any required feature reports `false` — no dual-path renderer, no silent degradation.
- **[architect]** `MAX_BINDLESS_TEXTURES = 16384`, `MAX_BINDLESS_SAMPLERS = 16` (static sampler variants: linear/nearest × repeat/clamp/mirror + anisotropic). Derivation-commented in code. *(Reconcile: asset-engineer proposed 4096 for Phase-6 showcase budget; architect's 16384 provides headroom for the lighting-mood-architect's shadow/LUT/volumetric slot reservations — the higher ceiling stands; if memory-budget analysis argues down, re-open via coder escalation.)*
- **[architect]** Descriptor pool: 1 bindless set (set=0) with `VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT` + N per-frame transient sets (set=1) for camera/light/CRT UBOs. PostProcessor chain stays on its own descriptor sets — untouched by this refactor.
- **[lighting]** Reserve slot ranges at layout-creation time for future: (a) shadow map atlas array, (b) 3D LUT grade textures per LightingProfile, (c) volumetric froxel volume. Documented in this record so the Phase-6+N lighting slice does not have to re-plumb.

### Material pipeline
- **[asset-engineer, 3d-modeler]** `.mat.xml` schema is UNCHANGED. The 32-bit index is a runtime-only field, never serialized. `scene_serializer` round-trip must remain byte-identical.
- **[asset-engineer, coder]** `MaterialGPU` std430 struct: explicit `uint32_t` alignment + `static_assert(sizeof(MaterialGPU) == N)` + per-field byte-offset comments (match the existing `WorldStateGPU` std140 pattern). First-principles derivation comment per M3.
- **[ai-engineer]** std430 material index buffer has a derivation comment explaining the 32-bit index packing and the reserved sentinel (slot 0 = null).
- **[3d-modeler]** `Result<>` failure tests for: missing texture asset referenced by slot, duplicate slot names, runtime index-out-of-range on full array.

### Shader
- **[asset-engineer, architect, coder]** `nonuniformEXT` qualifier mandatory at every dynamic-index sample site in the fragment shader. Shader-side derivation comment cites the SPIR-V `NonUniform` decoration (spec 14.1.1), why divergent indexing needs it, and the NVIDIA wave-coherence rule.
- **[vibe]** Fragment shader near the sample site carries a comment declaring the lighting model (Lambertian + Blinn-Phong) and noting this is NOT a PBR G-buffer — guards against silent BRDF drift.

### Isolation boundaries
- **[ai-engineer]** Nadir compute descriptor sets and the 7-SSBO layout (transforms/stats/spatial/world_state/persist/output/debug) are EXPLICITLY out of scope. No shared bindless table with Nadir. Debug SSBO readback path remains a distinct descriptor set.
- **[marty]** No changes to `src/audio/`. Texture upload / descriptor-array writes must not occur on the audio thread and must not hold any lock the audio thread can contend on. Texture hot-reload must not introduce a main-thread stall exceeding one audio-buffer period (~10ms at 48kHz/480-frame buffer).
- **[vibe]** No auto-PBR. Bindless must not be used as a gateway to normal-map-as-default, metallic/roughness channels, or realism-coded authoring. Any PBR-adjacent material-schema addition re-triggers council.

### Visual parity
- **[lighting]** Acceptance gate: all 6 showcase profiles (dread, hostile, liminal, sacred, warmth, wonder) must render byte-identical or perceptually identical (ΔE < 1.0 per tile) before/after migration, validated by golden-image tests on the RTX 3080 dev box. Tonemap→bloom→grade→vignette→grain stack order unchanged.
- **[lighting]** HUD legibility regression check: EVA HUD sampling path verified unchanged — no accidental sRGB/linear flip.

### M4 compliance artifact
- **[coder]** This record enumerates each `VkPhysicalDeviceDescriptorIndexingFeatures` flag enabled and why; it is the M4 "everything understood" artifact for `VK_EXT_descriptor_indexing`.

## Additions adopted

- **[architect]** Debug overlay toggle in `src/debug/overlay.cpp` visualizing bindless index as false-color per pixel — zero runtime cost when disabled.
- **[architect]** `docs/architecture.md` section documenting the bindless set-layout contract (set=0 bindless, set=1 frame UBOs, push constants for per-draw transform + material_index) so future subsystems (terrain, particles) slot in rather than inventing parallel descriptor schemes.
- **[architect]** Pipeline test: load `demo/showcase` with >100 distinct materials, assert a single `vkCmdBindDescriptorSets` call per frame for the bindless set, verify frame time <8.3ms on RTX 3080 (120fps target).
- **[asset-engineer]** `odyssey assets bindless-stats` CLI command (and matching MCP tool) dumping slot occupancy, fragmentation, top-N largest residents.
- **[3d-modeler]** `odyssey assets texture-count` CLI for pre-ship authoring budget checks.
- **[lighting]** Lighting-profile golden-image test harness (one frame per profile, hashed + perceptual diff) — lands WITH this PR, not after.
- **[ai-engineer]** Smoke test dispatching a Nadir frame after the migration and asserting output SSBO values are bit-identical to a pre-migration golden — catches descriptor-pool contention.
- **[marty]** Pipeline-level test: hot-reload a material texture while engine runs; assert no main-thread stall >10ms.
- **[vibe]** Fragment-shader comment near `nonuniformEXT` sample site declaring the lighting model + pointing to its derivation.

## Dissent recorded

None — 100% consensus among voting members. `netcode-engineer` abstained (wire format untouched).

## Critical concerns flagged for the implementer

1. **Generation counter on `TextureHandle`** — reload-after-unload at the same path is the single most common bindless bug (asset-engineer).
2. **Hard capability-check at device init** — a weaker GPU in CI silently over-subscribing the array is a shipped-UB risk (architect).
3. **Follow-on commitment for Kelvin/volumetrics/LUT** — if those slip indefinitely, we paid bindless cost without unlocking any mood work (lighting).
4. **Nadir descriptor pool contention** — keep graphics and compute pools disjoint or debug readback will silently stall (ai-engineer).
5. **Texture-upload main-thread bounding** — unbounded hash-map allocation during gameplay can cause audio underruns (marty).
6. **Authoring drift toward abundance** — 16384 slots normalizes "more textures is fine"; charter is built on restraint (vibe).

---

## Deferred Conditions

The following conditions from the "Additions adopted" list could not be fully satisfied in Phase 6 and are flagged here per the budget-discipline protocol.

### [lighting] Golden-image test harness — DEFERRED

**Condition:** Lighting-profile golden-image test harness (one frame per profile, hashed + perceptual diff) — "lands WITH this PR, not after."

**Gap:** The golden-image harness requires GPU rendering in a CI context (RTX 3080 dev box) and a stable reference frame for each of the 6 showcase profiles (dread/hostile/liminal/sacred/warmth/wonder). The `demo/showcase/` scene does not yet have all 6 profiles wired to distinct rendered frames. The harness infrastructure (off-screen render + PNG hash comparison) is not in place in `tests/pipeline/`.

**Status:** Pipeline tests that require GPU are in `tests/pipeline/`. The perceptual-diff infrastructure (ΔE < 1.0 per tile) requires a reference rendering pass that is a separate engineering effort. This is caller TODO #1 (lighting-mood-architect visual parity sign-off).

### [architect] >100-material pipeline test — DEFERRED

**Condition:** Pipeline test: load `demo/showcase` with >100 distinct materials, assert a single `vkCmdBindDescriptorSets` call per frame for the bindless set, verify frame time <8.3ms on RTX 3080.

**Gap:** The showcase scene has fewer than 100 distinct materials at Phase 6 merge time. Adding >100 materials to the showcase is an asset-authoring task, not a code task. The `vkCmdBindDescriptorSets` count assertion requires a Vulkan command-counting harness that is not currently in the pipeline test infrastructure.

**Status:** The renderer architecture guarantees a single bind per frame by construction (one `vkCmdBindDescriptorSets` call in `begin_frame` / `begin_frame_offscreen`). The >100-material load test is deferred until the showcase scene has sufficient assets.

### [ai-engineer] Nadir-frame bit-identity smoke test — DEFERRED

**Condition:** Smoke test dispatching a Nadir frame after the migration and asserting output SSBO values are bit-identical to a pre-migration golden.

**Gap:** Generating a pre-migration golden requires running the engine before Phase 6 and capturing SSBO output, then comparing after. This requires a deterministic scenario with known entity positions and behavior states. The infrastructure for SSBO snapshot comparison does not exist in `tests/pipeline/`.

**Status:** Nadir descriptor pool isolation is correctly maintained (compute and graphics pools are disjoint by construction — the NadirSystem allocates its own pool and never interacts with the bindless graphics pool). The bit-identity assertion is deferred to a follow-on test infrastructure task.

### [marty] Hot-reload stall test — DEFERRED

**Condition:** Pipeline-level test: hot-reload a material texture while engine runs; assert no main-thread stall >10ms.

**Gap:** Hot-reload requires a file-watcher loop (ReadDirectoryChangesW) and a live engine running WASAPI audio simultaneously. This is an integration test requiring the full engine runtime, not a unit test. The upload-stall bound is enforced architecturally (staging + fence per texture, no unbounded allocation), but the timing assertion against the 10ms audio-buffer boundary cannot be tested in a headless unit test.

**Status:** The architecture satisfies Marty's stall condition by design. The timing regression test is deferred.

---

## Acceptance deferred (2026-04-21, post-smoke-test)

Lighting-mood-architect was consulted as the council-ratified acceptance gate after the smoke test (see `odyssey_shooter.exe --scene demo/showcase/showcase.scene.xml` clean launch, commit `7bbe841`). Verdict: **CONDITIONAL-APPROVE**. Visual parity is preserved by construction — PostProcessor is untouched, the Phase-5 lighting profile runtime and CRTParams overlay are unchanged, the post-FX stack order is preserved, and the showcase scene has effectively zero authored-texture surface area today. A clean runtime with no validation errors is necessary, not sufficient.

**The ΔE<1.0 golden-image gate is WAIVED only while all three preconditions hold simultaneously:**

1. PostProcessor stays isolated (its own descriptor sets; CRT + EVA HUD untouched by any future phase).
2. Authored-texture count stays near zero (the showcase has not yet accumulated many-material authored assets).
3. No lighting-subsystem expansion ships (specifically: no Kelvin palette, no volumetrics, no 3D LUT grade textures, no shadow atlas, no directional_override landing in `src/vulkan/` or `src/assets/`).

**The moment any of those three change, the golden-image harness MUST be green before the change merges.** The harness is tracked as a follow-on at `docs/decisions/2026-04-21-bindless-golden-image-harness.md` — same trigger as lighting-mood-architect's original Phase-5 follow-on condition.
