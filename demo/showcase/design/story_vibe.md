# Showcase — Story / Vibe contribution

**Agent:** vibe-story-guardian
**Scope:** the felt experience of the showcase scene. The showcase is a technical demo, not a narrative game — but even a demo has a register, and this one has to embody the Vibe Charter or the engine's thesis goes unproven.
**Charter reference:** `docs/vibe_charter.md` v0.

---

## 1. The felt-experience thesis

The showcase must read as **an arena the world was already running inside when the player arrived, and will keep running inside after the player leaves.** Not a level. Not a test room. A weather system the player can walk into.

Everything that follows is in service of that single reading.

**One-line pitch (non-shipping):** *You step into a stone-walled arena at dusk. The world is already tense. Nothing explains itself. You act, or you watch — both are legitimate.*

---

## 2. Atmospheric arc (no cutscenes, no script)

The showcase has a three-beat arc driven entirely by entity behavior + MusicDirector state + lighting. No dialogue, no voice-over, no on-screen text beyond mechanical necessity.

| Beat | Duration | What the world is doing | MusicDirector state | Lighting register |
|---|---|---|---|---|
| **1. Arrival** | first ~15 s | Civilians wander the arena. Hunters patrol their flanks. Snipers crouch on the rise. The boss gunner is still. Nothing is aimed at the player yet. | `explore` (low, breathing bed layer only) | Liminal — dusk Kelvin, long shadows, torches flickering but not dominant |
| **2. Recognition** | triggered by first hostile LOS or first shot | Hunters cohere toward the player. Civilians flee outward. Snipers pivot. The boss has not moved — it is *watching*. | crossfade to `combat` over 2 s, intensity rises with pack proximity | Kelvin drops ~400 K cooler, fog density +15%, torches assert themselves |
| **3. Aftermath** | when all hostiles eliminated OR player down | Surviving civilians slow. Music collapses into its final-cadence tail. The arena holds. | `victory` (a resolution, not a fanfare) OR `fail` (a single low sustained tone, no sting) | Kelvin warms marginally; fog stays; no "reward" color pop |

Pillar alignment:
- Beat 1 honors **Pillar 1** (weather-like parallelism): the player walks into a world mid-computation, not a world waiting on a trigger.
- Beat 2 honors **Pillar 6** (silence carries the register): the transition to combat is *earned* by the player's first hostile act, not telegraphed with a stinger-on-entry.
- Beat 3 honors the **Sacred Element: the player's death.** The `fail` music cue is a single low tone, no orchestral hit, no "you died" text. The arena remains.

---

## 3. Zone-by-zone mood mapping

The arena has no loading zones, but it has compositional regions. Each region's mood supports a specific pillar.

| Region | Entities | Pillar it proves |
|---|---|---|
| **Arena center (gold pillar)** | gold_leaf pillar + painted-wood crates | **Pillar 3 (legibility over spectacle).** The gold-leaf pillar is a single high-contrast vertical read against hand-painted wood and matte stone — no photoreal pretension, one anchor for the eye. |
| **North/south/east/west stone walls** | pillar-mesh walls, stone_wall PBR material | **Pillar 4 (first-principles).** The PBR here is the PBR *we wrote*. It can be audited shader-line by shader-line; no Standard-Material fallback. |
| **Left / right flanks (hunter packs)** | 8+8 enemy_pack_hunter archetypes | **Pillar 1 (parallel weather).** Two mirror packs run the same Nadir shader on separate SSBO sets and cohere *around* their leaders without a controller. You can watch them diverge. |
| **Rear rise (snipers)** | 4 enemy_ranged at y=5 | **Pillar 5 (physicality).** Ranged enemies stand still, steady, and commit to shots. Their weight is the opposite of the hunters' motion. |
| **Far end (boss gunner)** | multi_arm_gunner, scale 1.5 | **Sacred / Pillar 2 (no explanation).** The boss is a physically larger thing with no name-card, no health bar stretched across the top of the screen, no lore prompt. It is simply *more than the others*, and the player infers that. |
| **Interior drift (civilians)** | 12 civilian_fleeing | **Pillar 1 + Sacred (silence).** The civilians do not have plot significance. They are the arena breathing. A player who never fires a shot still watches a world in motion. |
| **Overhead (sun) + torches + flashlight** | 1 directional + point + spot | **Pillar 3 + lighting charter.** Three light types, three Kelvins (5500 / 1900 / 4200) — a single composition, not a showcase reel of effects. |

---

## 4. Anti-touchstones — things this showcase MUST NOT resemble

If a reviewer could mistake this showcase for any of the following, the vibe has been lost. These are in addition to the charter's project-level anti-touchstones.

1. **A UE5 tech demo reel.** No Nanite-perfect stone, no Lumen "look how it bounces" setpiece, no camera orbit around a shader ball. The camera is the player's. Period.
2. **A Unity asset-store horde arena.** No tinted-red "enemy waves incoming" flash, no XP pop-ups, no round-clear fanfare, no combo counter.
3. **A military shooter training map.** No orange-to-blue contrast grading, no hit markers, no kill-confirm sound, no ammo-low radio barks.
4. **An Overwatch-style hero arena.** No character silhouettes lit in team colors, no ability-ready pings, no chirpy "play of the game" framing.
5. **A Soulslike homage.** No low-poly fog-veiled cathedral, no item-pickup gravitas stings, no "YOU DIED" card. The charter already forbids the card; the *posture* of reverent-suffering is adjacent territory we stay out of.
6. **A procgen sandbox.** The arena is hand-placed. The world's richness comes from the behaviors running inside it, not from a seed.

`/anti-touchstone-check` will scan each authored asset against this list at review.

---

## 5. Diegetic text, dialogue, and sound cues

Strict minimum. The charter forbids lore dumps, quest text, and ironic voice-over. The showcase inherits that. The only permitted non-mechanical text surfaces are:

| Surface | Content rule | Rationale |
|---|---|---|
| **Window title** (existing) | `HP / Ammo / Score / Enemies` — numeric, no flavor text. | Mechanical necessity. Charter **Forbidden #6** permits this; it does not permit more. |
| **EVA HUD overlay** (existing) | `health_pct`, `alert_level`, `sync_ratio` as visual registers — no prose. | Pillar 3. The HUD is tonal. See: Evangelion touchstone. |
| **One-sentence mechanic hint (first-use only)** | e.g. "Left mouse to fire." Never more. Never decorated. | Charter **Forbidden #6**. |
| **Names of archetypes** (in debug / editor only) | `enemy_pack_hunter`, `civilian_fleeing` — functional, not lore. | Pillar 2. No "the Fallen," no "the Kindred." They are what they do. |
| **Music state names** (in MusicDirector) | `explore`, `combat`, `victory`, `fail` — functional. | Same. |

**No dialogue. No barks. No narrator.** The civilians do not cry out when fired upon. Their deaths are silent except for the Nadir-requested SFX that the composer routes (see composer contribution). Silence is the register. See **Pillar 6** and **Sacred Element: silence**.

**Sound cue inventory (only these, nothing else):**
- Ambient bed (arena_ambience.ogg) — always.
- Per-archetype leitmotif fragments, triggered by the composer's `/leitmotif-bind` when an entity enters the player's awareness.
- One stinger on first hostile LOS (composer's `/compose-stinger`) — this is the *recognition* beat.
- Death SFX (routed from Nadir sound_request) — mechanical only, no screams, no "noble fallen hero" crescendo.
- Final-cadence tail for victory or a single low sustained tone for fail.

No elevator-music combat loop. No "enemy killed" ding.

---

## 6. Player posture

**How we want the player to sit while playing this:**

- Forward-leaning, but not frantic.
- Alert, not anxious.
- Willing to stop moving and *watch* the hunters flank the wrong angle for six seconds before re-engaging — because the world rewards the watcher.

If the showcase produces a "twitch-shooter" posture (leaned back, spraying), the scoring/damage tuning has overpowered the register. That is a drift signal. See `vibe_acceptance.md`.

---

## 7. What the showcase is *about* (the one-sentence truth, applied)

Under the mechanics, this demo is about **the difference between a world and a level.** A level waits for you. A world does not. Every design decision in this scene — parallel Nadir dispatch, civilians with no mission relevance, a boss that does not move until you threaten it, silence where a lesser demo would put music — is pointed at proving that difference in twenty seconds of play.

If a reviewer cannot articulate that difference after playing the showcase once, the vibe contribution has failed, regardless of whether every checkbox in `acceptance_report.md` is green.
