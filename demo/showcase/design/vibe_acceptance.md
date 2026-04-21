# Showcase — Vibe acceptance checklist

**Agent:** vibe-story-guardian
**Skills used:** `/charter-check`, `/anti-touchstone-check`, `/vibe-audit`.
**Target:** every prefab, material, theme, light, post-FX pass, music state, and Nadir archetype contributed by any other agent passes charter review before showcase sign-off.

The showcase ships only if every row below is PASS.

---

## 1. Per-asset `/anti-touchstone-check` gate

Every file authored under `demo/showcase/` is scanned. Fail condition: the asset reads as one of the six anti-touchstones from `story_vibe.md` section 4, or any of the five charter-level anti-touchstones.

| Asset class | Path glob | What I'm looking for |
|---|---|---|
| Materials | `demo/showcase/materials/*.mat.xml` | Not photoreal-chasing. `stone_wall` PBR is defensible shader-line by shader-line (**Pillar 4**). `painted_wood` is saturated and matte — the anti-realism touchstone the asset doc calls out. `gold_leaf` is a single high-contrast read, not a "shader ball showcase." |
| Meshes | `demo/showcase/meshes/*.mesh.xml` | Primitive + hand-placed. No Nanite-grade imports pretending to be hero props. |
| Prefabs | `demo/showcase/prefabs/*.prefab.xml` | No ability icons, no class badges, no UE5-marketplace silhouettes. |
| Lighting profile | `demo/showcase/lighting_profiles/Liminal.xml` | Kelvin palette respects Pillar 3. No orange-teal Hollywood grade, no Lumen signature look. Flicker curve on torches reads as atmospheric, not decorative. |
| Post-FX chain | `demo/showcase/lighting_profiles/*` + `shaders/crt_postprocess.*`, `shaders/eva_hud.frag` | CRT + EVA HUD + fog, in that order. No SSS skin, no TAA smearing, no lens-dirt overlay (**Forbidden #1**). |
| Music | `demo/showcase/music/showcase.music.xml` + stems | `explore / combat / victory / fail` states; `fail` is a single low tone, never a sting. Stingers punctuate recognition, not decorate entry (**Pillar 6**). |
| Nadir archetypes | `demo/showcase/behaviors/*.nadir` | Parallel dispatch preserved. No sequential fallback disguised as a headline behavior (**Sacred: parallelism**). |
| Scripts | `demo/showcase/scripts/*.cpp` | No tutorial text beyond one sentence per mechanic on first use (**Forbidden #6**). No ironic voice in any string literal. |

Run: `/anti-touchstone-check <path>` per asset. Output: `PASS` or `FLAG <anti-touchstone> <why>`. Every asset must return `PASS`.

---

## 2. Per-pillar `/charter-check` gate

For each pillar, I verify the showcase exhibits a concrete demonstration of it. If a pillar is not demonstrated, the showcase is a technical pass but a vibe fail.

| # | Pillar | How the showcase demonstrates it | Who owns the evidence | Gate |
|---|---|---|---|---|
| 1 | Weather-like parallelism | Two mirror hunter packs + civilians + snipers + boss all dispatch in the same Nadir frame; `/inspect-scoring` on any one shows independent per-entity scoring | game-ai-engineer | PASS / FAIL |
| 2 | No in-world explanation | Zero lore text in scene. Boss has no nameplate. Archetype names functional only. | vibe-story-guardian (me, by walkthrough) | PASS / FAIL |
| 3 | Legibility over spectacle | Gold pillar reads at a glance; CRT+EVA HUD is tonal not cosmetic; no additive-bloom hero shot | lighting-mood-architect + me | PASS / FAIL |
| 4 | First-principles code | Every material's shader is ours; rigid body is ours (not Jolt); PBR terms are defensible on a whiteboard | game-engine-architect | PASS / FAIL |
| 5 | Player body has weight | FPS player has commit-cost on shots (0.15 s cooldown holds), IK on humanoid enemies, hit-stop frames not floaty | game-engine-architect + game-asset-engineer | PASS / FAIL |
| 6 | Silence carries | Opening 15 s is bed-layer only; recognition stinger is earned; `fail` cue is one low tone | marty-odonnell-composer | PASS / FAIL |
| 7 | XML-authorable | Every asset round-trips byte-identical through the serializer; CLI can load the scene without the editor running | game-asset-engineer | PASS / FAIL |

Run: `/charter-check "Showcase demonstrates Pillar <n> via <evidence>"` per row. Accept only `BLESSED`. `RESHAPE` means go fix before ship. `REJECTED` means convene council.

---

## 3. Forbidden-list scan (hard fail)

These eight are non-negotiable. One hit = the showcase does not ship.

- [ ] No photorealism-chasing post-FX anywhere in the chain.
- [ ] No cargo-culted PBR (every term defensible).
- [ ] No opaque third-party lib pulling weight in a core system.
- [ ] No quest markers, waypoints, or objective arrows.
- [ ] No damage numbers. No kill feed. No hit markers.
- [ ] No in-game tutorial text beyond one sentence per mechanic on first use.
- [ ] No loot-box / gacha / live-service grammar (pity timer, daily quest, FOMO UI).
- [ ] No register-breaking humor in any string, asset name, or audio cue.

Evidence: grep the scripts dir and scene XML for likely offenders (`damage_number`, `kill_feed`, `waypoint`, `xp_gain`, `daily_`, `streak_`). Walkthrough: play through once end to end, look for any of the above rendered on screen.

---

## 4. Sacred-element gate

Three checks, each a single failure point.

- [ ] **Player death register.** Die deliberately during acceptance. The moment must not be played for laughs, must not reward with loot, must not be annotated with a "YOU DIED" card. Music drops to the single low tone. Window title preserves HP=0.
- [ ] **Silence permission.** Stand still in the arena for 30 seconds without shooting. The music bed does not escalate. No "are you still there?" prompt. No combat-drift into music.
- [ ] **Nadir parallelism.** `/inspect-scoring` on any hunter shows all 8 pack members scored in the same dispatch; no CPU-fallback serial loop. This is the engine's thesis; a regression here is a ship-block regardless of visual polish.

---

## 5. Drift watch (running `/vibe-audit`)

I run `/vibe-audit demo/showcase/` as a whole before every acceptance run. It returns a ranked drift list. Any drift at severity HIGH or above = block. MEDIUM = file and fix before next run. LOW = log for the change log.

Known drift patterns to watch for in this project specifically:

- **"Just one quest marker"** creep — someone adds a debug arrow and forgets to remove it. **Forbidden #4.**
- **"It looks flat without TAA"** — someone enables temporal AA to hide aliasing. **Forbidden #1.** Fix the aliasing, not the image.
- **"The combat music should kick in on entry"** — someone triggers `combat` on scene load instead of on first hostile LOS. Violates **Pillar 6** and the Arrival beat.
- **"Can we show 'WAVE 1'?"** — arena-genre convention creep. Anti-touchstone #2.
- **Civilians with names** — someone gives a civilian a unique ID that reads as a character. Breaks Pillar 2 and the "arena breathing" role.

---

## 6. Ship / hold decision

**SHIP** the showcase when: every row in sections 1–4 is PASS, and `/vibe-audit` returns no HIGH drifts.

**HOLD** if: any Forbidden hit, any Sacred failure, any unresolved HIGH drift, or `/charter-check` returns REJECTED on a pillar row.

**Council escalation** per the council protocol if an author disputes a HOLD.
