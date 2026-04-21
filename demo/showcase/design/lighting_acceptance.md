# Showcase — Lighting Acceptance Checklist

**Judge:** lighting-mood-architect
**Charter binding:** `docs/vibe_charter.md` v0. A PASS here means the lighting layer both reads correctly (mood-wise) and behaves correctly (engine-wise). A single mood failure or a single engine failure is a FAIL for the lighting row.

Run order is left-to-right; a fail short-circuits the rest.

---

## A. Kelvin palette adherence

For each of the six zones, run `/kelvin-preview <K>` for **every** authored Kelvin value (light intensities, fog color_kelvin, directional_override kelvin). Confirm:

- [ ] Every Kelvin lies in the `<palette>` primary or secondary band of its zone's profile XML (or is an explicit secondary if the light is flagged as such).
- [ ] No authored Kelvin falls inside a zone's `<disallowed_sources>` band.
- [ ] The `/kelvin-preview` hex output for the sun per-zone matches the zone's `directional_override` color (visual sanity check against the swatch).
- [ ] All light caption classifications align with the zone's intended mood class (e.g. a Sacred light classified as "Alienation / clinical" is a FAIL).

Spot checks required:
- Sacred: `altar_candle_L` at 1850 K → must classify as "Sacred secondary, Warmth".
- Liminal: `lab_fluorescent` at 4000 K → must classify as "Liminal (clean)".
- Dread: `pit_boss_emissive` at 10000 K → must classify as "Alienation".

---

## B. Light count budget

Run `/descriptor-dump` with the game paused on each zone. For each zone:

- [ ] On-screen dynamic light count ≤ 8 (LightBuffer SSBO budget).
- [ ] Total scene LightBuffer entries = 14 (or current roster count from `lighting.md` §2 — recount on regression).
- [ ] Zero `VK_NULL_HANDLE` entries in the populated slot range.
- [ ] Slot indices referenced by the lighting shader match the CPU-side bind order (cross-check against CPU registration log).

Zone-by-zone expected active counts (sun + flashlight + player_aura + zone-local):
- North Altar: 5 (sun, flash, aura, candle_L, candle_R)
- West Chapel: 4 (sun, flash, aura, chapel_godray_key)
- South Pit: 8 (sun, flash, aura, boss_emissive, ember ×4) — at cap, do not regress
- East Lab: 4 (sun, flash, aura, lab_fluorescent)
- NE Courtyard: 4 (sun, flash, aura, courtyard_pulse)
- SW Hearth: 6 (sun, flash, aura, firepit, lantern_1, lantern_2)

Peak-zone alarm: if South Pit exceeds 8, kill the `pit_ember_area` 4-sample scheme and swap to a single virtual point; file a council issue for a real area-light primitive.

---

## C. Flicker determinism

`/light-flicker-tune` writes deterministic noise parameters. To audit:

- [ ] For `torch_north`, run `/light-flicker-tune torch_north --amplitude=0.15 --frequency=6 --seed=0xA17C4`, record the LightBuffer row. Restart the engine. Re-run the same command at the same tick index. Rows must be **bit-identical**.
- [ ] Same test for `pit_boss_emissive` (seed `0xB055`), `altar_candle_L` (seed `0xA17A8`), `hearth_firepit` (seed `0xF17E0`).
- [ ] Verify `flicker(tick_index, seed)` uses `tick_index` (uint64), not wall-clock or delta-time. A replay hash divergence on a flickering light's intensity field is a FAIL.
- [ ] `/replay-record` for 60 s with all flickers active, then `/replay-play` — per-tick state hashes must match frame-for-frame.

---

## D. Fog-per-zone correctness

For each zone, confirm via live inspection (run the showcase, walk to each zone):

- [ ] Fog density matches profile XML `<fog density>` to within 1e-4 at steady state.
- [ ] Fog color temperature visually matches the Kelvin hex from `/kelvin-preview` on the profile's `color_kelvin`.
- [ ] `<fog type>` enum drives the correct shader path — `exponential_height` reduces fog with altitude; `exponential` is uniform.
- [ ] Volumetrics are **off** in Liminal and Hostile (confirmed by profiling: volumetric pass is ~0 ms or skipped).
- [ ] God rays appear only in Sacred and Wonder (match profile `<volumetrics god_rays="1"/>`).
- [ ] Volumetric ray-march step count matches profile value (debug overlay exposes this).

---

## E. Bindless descriptor validation

`/descriptor-dump` output cross-referenced against the runtime shader-side indexing:

- [ ] `LightBuffer` SSBO is bound exactly once, and its slot index matches `LIGHT_BUFFER_BINDING` in the lighting shader.
- [ ] Every light slot in the populated range reports a non-null position and non-zero intensity range.
- [ ] The **grade LUT** (3D texture) for the active profile is bound to the post-FX descriptor set and the grade shader reads that same slot.
- [ ] The **volumetric 3D texture** is bound when the active zone has volumetrics on, and unbound (or flagged `PARTIALLY_BOUND`) when off — the shader must guard against sampling an unbound slot.
- [ ] No descriptor slot carries `VK_NULL_HANDLE` in the range `[0, allocated_count)`.
- [ ] Sampler array contains at least point, linear, and anisotropic (shared with renderer).

---

## F. Post-FX barrier audit

`/barrier-audit` run in steady state of the active zone:

- [ ] Pipeline barrier sequence matches: offscreen → tonemap → bloom → grade → vignette → grain → CRT → EVA → swapchain. No out-of-order edges.
- [ ] Explicit image layout transition `COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL` appears between offscreen and tonemap.
- [ ] Grade-LUT descriptor update on zone transition is surrounded by `VK_PIPELINE_STAGE_HOST_BIT` or uses `UPDATE_AFTER_BIND` — no implicit sync.
- [ ] `offscreen_complete` and `postprocess_complete` semaphores are both present and distinct.
- [ ] No "implicit" edges reported by `/barrier-audit --strict`.
- [ ] CRT and EVA HUD stages appear last and are engine-owned (no profile XML ever touches them).

---

## G. Vibe-audit / charter check

- [ ] `/vibe-audit demo/showcase/lighting_profiles/` returns 0 severity-high drifts.
- [ ] `/anti-touchstone-check` on each profile returns PASS (no UE5-default, no PBR-by-default, no photoreal, no lens-dirt).
- [ ] For each zone, the dominant light has a **narratively plausible source** (sun, candle, fluorescent ceiling, pulsing alarm, hearth fire, boss-creature emissive). `player_aura` is the only un-sourced light and is held under 2.0 intensity by design.
- [ ] At least one zone leaves the player in meaningful darkness (restraint check — Pillar 3). Expected: Dread.

---

## H. Regression safeguards

- [ ] `/roundtrip-test demo/showcase/showcase.scene.xml` — byte-identical on re-serialize even with lighting attributes added.
- [ ] `/validate-asset` on all six profile XMLs returns `VALID` (pending `schemas/lighting_profile.xsd` — noted as pipeline dependency in `lighting.md` §8).
- [ ] GPU frame time delta on RTX 3080 per zone stays inside budget: Sacred/Wonder ≤ +1.2 ms (volumetrics cost), Dread ≤ +0.8 ms, Warmth ≤ +0.5 ms, Liminal/Hostile ≤ +0.2 ms (no volumetrics).
- [ ] HUD legibility survives every profile — EVA HUD alert-level text is readable against each zone's backdrop. Run all six, capture screenshots, visually verify.

---

## Pass / fail decision

**PASS:** every box checked across A–H for the run.
**FAIL:** one or more boxes unchecked. File the specific row as a council triage item with the profile name, light id, and the skill invocation that surfaced the issue.
