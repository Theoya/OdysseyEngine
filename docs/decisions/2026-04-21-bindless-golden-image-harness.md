# Bindless golden-image harness (follow-on to Phase 6)

- Date: 2026-04-21
- Parent decision: `docs/decisions/2026-04-21-bindless-refactor.md` (Phase 6 bindless refactor, RATIFIED at 100% consensus)
- Status: **PENDING** — no implementation yet; this file pins the contract.
- Owner: **lighting-mood-architect** (acceptance-gate holder) + **council-implementation-coder** (implementation)

## Why this exists

Phase 6 (bindless descriptor refactor) merged under a CONDITIONAL-APPROVE from lighting-mood-architect. The decision record's acceptance gate —

> "All 6 showcase profiles (dread, hostile, liminal, sacred, warmth, wonder) must render byte-identical or perceptually identical (ΔE < 1.0 per tile) before/after migration, validated by golden-image tests on the RTX 3080 dev box."

— could not be automated at Phase-6 merge time because (a) no GPU CI infrastructure, (b) the `demo/showcase/` scene has minimal authored-texture surface area, and (c) the `tests/pipeline/` harness has no perceptual-diff tooling. Visual parity was waived by construction (PostProcessor untouched, lighting-profile runtime unchanged, no authored textures in scope) with three preconditions that keep the waiver valid.

## Hard precondition / trigger

**This harness MUST be green before ANY of the following merges to `main`:**

1. The Kelvin palette / volumetrics / directional_override / 3D LUT grade / shadow atlas subsystem (the full lighting slice deferred from Phase 5 and pre-committed as lighting-mood-architect's Phase-5 follow-on).
2. Any change that loosens the PostProcessor isolation boundary (touches the post-FX descriptor sets, CRT chain, or EVA HUD sampling path).
3. Any authored-texture growth phase that brings the showcase above ~20 distinct material textures (round number — re-evaluate ceiling when approaching).
4. Any bindless-adjacent refactor (bindless storage buffers, mesh indexing, async uploads) on top of Phase 6.

## Scope

What the harness MUST do:

1. **One reference frame per profile.** Render the showcase scene at a fixed camera, fixed time-of-day, fixed entity state, under each of the 6 profiles (dread, hostile, liminal, sacred, warmth, wonder). Capture the offscreen render target (pre-swapchain) as PNG.
2. **Perceptual diff.** ΔE*2000 (or ΔE*94 as fallback) per 8×8 tile, threshold < 1.0 against a committed reference PNG set under `tests/fixtures/golden_images/phase6_baseline/`.
3. **Headless-capable** if the project ever adds GPU CI. Until then, manual-run on RTX 3080 dev box via `odyssey_tests_pipeline.exe --gtest_filter=LightingProfileGolden.*`.
4. **Bit-identity for CRT/EVA HUD samples.** A companion test confirms the CRT shader's `brightness`/`vignette_strength`/`flicker_amount`/`chromatic_aberration` uniforms are bit-identical before/after (already provable from code inspection; add an assertion test).

What it does NOT do:

- Runtime perf regression (different harness).
- Asset authoring (a separate track).
- Nadir SSBO bit-identity (separate deferred condition from ai-engineer).

## Implementation path

1. Land a deterministic camera + entity-state freeze mode for `odyssey_tests_pipeline.exe` (likely a new `--fixture=phase6-baseline` flag).
2. Render each of the 6 profiles, capture the PostProcessor offscreen target via `vkCmdCopyImageToBuffer` → host readback → PNG write.
3. Commit the 6 reference PNGs (one per profile) to `tests/fixtures/golden_images/phase6_baseline/`. Image hash (SHA-256) of each PNG goes in a sidecar JSON for fast-fail detection before expensive ΔE.
4. Perceptual diff: implement ΔE*2000 (or fall back to ΔE*76 if time-constrained; document the choice + derivation per M3).
5. Wire as a GTest suite under `tests/pipeline/test_lighting_profile_golden.cpp`.

## First-principles math (M3 note for the implementer)

ΔE*2000 is the CIE-recommended perceptual color-difference metric; it corrects known ΔE*76/ΔE*94 shortcomings in the blue region and near neutral. Derivation: convert sRGB → linear → XYZ → CIE L*a*b*, then apply the ΔE*2000 formula (which involves weighting functions for lightness, chroma, and hue differences, plus a rotation term near blue hues). The formula and constants come from CIE Publication 142:2001; cite the publication in the derivation comment. If ΔE*2000 proves too heavy for per-tile evaluation, fall back to ΔE*76 (straight Euclidean in L*a*b*) with an adjusted threshold (typically ×1.5 since ΔE*76 over-reports differences).

## Budget

One implementation phase. Not to exceed one council-implementation-coder delegation cycle. If it sprawls beyond that, escalate back to council for scope trimming.

## How to close this file out

When the harness is green on the RTX 3080 dev box against all 6 profiles:
1. Update the **Status** line above to "RATIFIED" with the commit SHA landing the harness.
2. Append a "Closed" section summarizing the ΔE numbers seen for each profile.
3. Retire the "Acceptance deferred" block in `docs/decisions/2026-04-21-bindless-refactor.md` by appending a closing note pointing here.
