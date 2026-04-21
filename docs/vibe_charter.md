# OdysseyEngine — Vibe Charter

**Version:** v0
**Last amended:** 2026-04-20
**Status:** Living document. Amend via `/charter-edit`. Forbidden list changes require council.

---

## One-Sentence Truth

*A game where the world is computed all at once — like weather, like a thought — and the player moves through it as a witness who is also implicated.*

---

## Emotional North Star

**Reverence with stakes.**

The player leaves a session feeling that something larger-than-them was happening whether they participated or not, and that their participation *mattered* inside it but did not *resolve* it.

---

## Pillars

1. **We always let the world compute itself in parallel so that it feels weather-like, not script-like.**
   Nadir evaluates all behaviors simultaneously. The world was not waiting for the player.

2. **We always refuse to explain the world in text so that meaning is inferred, not delivered.**
   No lore dumps, no tooltips beyond mechanical necessity, no quest markers. The player's questions are the product.

3. **We always prefer legibility over spectacle so that every moment reads as a composition, not an effect.**
   Post-FX serves atmosphere, not marketing. The CRT + EVA HUD layer is tonal, not cosmetic.

4. **We always build every system from first principles in code we wrote so that nothing in the engine is a black box.**
   No cargo-culted PBR. No "use the standard engine X." The engine's look is inseparable from its mechanics.

5. **We always treat the player's body as an instrument of physical weight so that movement feels inhabited, not piloted.**
   Skeleton, IK, animation rhythm, and input commitment serve physicality. FPS does not mean floaty.

6. **We always let silence carry the register so that sound, when it arrives, means something.**
   Music is adaptive and earned. Stingers punctuate; they do not decorate.

7. **We always build tools that a reader-of-XML can author with so that the project stays legible to humans and agents alike.**
   XML assets, CLI-first, MCP-addressable. No binary-only authoring workflows in the core pipeline.

---

## Touchstones

- **Shadow of the Colossus** — worlds that exceed the player; physicality as reverence; refusal to explain.
- **NieR: Automata** — thematic ferocity in a pop framework; endings that recontextualize rather than conclude.
- **Outer Wilds** — a simulated world that runs without the player and is better for it; knowledge as the reward.
- **Neon Genesis Evangelion (TV series)** — UI-as-emotion; the HUD is part of the performance.
- **Halo: CE (Marty O'Donnell score)** — choral adaptive music that swells with threat and memory, not with spectacle.

---

## Anti-Touchstones

- **Modern AAA photorealism (e.g., Hellblade II-style face pipelines)** — expensive realism that displaces atmosphere with fidelity. Not the register.
- **Unreal Engine 5 marketplace defaults** — Nanite-perfect rocks, Lumen-lit corridors, PBR-by-default materials. The hazard is looking like every other UE5 indie by accident.
- **Live-service UI conventions** — quest markers, waypoints, damage numbers, toast notifications. Cognitive load that displaces attention with instruction.
- **Joss-Whedon-voice dialogue** — self-aware quipping that collapses reverence.
- **Procedural-generation-as-content (No Man's Sky v1 trap)** — infinite surface, no thematic density. Opposite of weather-like.

---

## Forbidden (hard no's)

1. **No photorealism-chasing post-FX.** SSS skin, TAA smearing, lens-dirt overlays for "realism."
2. **No cargo-culted PBR.** If a material uses PBR we wrote the shader and can defend every term.
3. **No opaque third-party libraries in core systems.** Well-scoped deps (Vulkan, GLFW, shaderc, pugixml) are fine; libraries that own behavior or rendering semantics we can't read and modify are not.
4. **No quest markers, waypoints, or on-screen objective arrows.** The world teaches.
5. **No damage numbers or kill feeds.** Combat communicates through feel, animation, and audio — or it fails.
6. **No in-game tutorial text beyond one sentence per mechanic on first use.**
7. **No loot-box, gacha, or live-service monetization grammar** — pity timers, daily quests, FOMO UI — even vestigially.
8. **No jokes that break the register.** No ironic-distance humor, no characters winking at the audience.

---

## Sacred Elements

- **The player's death.** Never played for laughs; never incentivized by a meta-reward. Death is the register of failure and is allowed to be heavy.
- **Silence.** Do not fill it because you fear it.
- **The Nadir system's parallelism.** The "weather-like" quality is the game's thesis; sequential fallbacks that betray it are not acceptable for headline behaviors.

---

## Change Log

- **v0 — 2026-04-20** — Initial draft by vibe-story-guardian, seeded from existing repo signals (Nadir architecture, pure-function mandate, CRT+EVA post-FX, Halo-lineage composer agent, FPS humanoid demo, user's stated "Fantasy Etherealism Impressionism" register, "everything understood" engineering principle). Awaiting user amendment. Council: n/a (initial creation).
