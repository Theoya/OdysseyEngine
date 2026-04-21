# OdysseyEngine Showcase Demo

**Purpose:** a single, canonical scene set that exercises every engine feature so each council agent can audit their domain end-to-end. When any feature regresses, it fails here first.

**Scope:** Windows only. Lives at `demo/showcase/`.

## Master scene
- `showcase.scene.xml` — the single canonical scene
- `prefabs/` — per-archetype prefabs (one per feature family)
- `materials/` — exercises every material attribute type
- `meshes/` — primitive + imported mesh coverage
- `behaviors/` — per-archetype `.nadir` shaders
- `actions/` — `.actions.xml` serial sequences
- `animations/` — `.skeleton.xml` + `.anim.xml` pairs
- `lighting_profiles/` — mood preset XML
- `music/` — `.music.xml` state machine + stems directory
- `scripts/` — C++ script bindings (registered via REGISTER_SCRIPT)

## Feature coverage matrix
Each entry: [feature] → [responsible agent] → [showcase entity/file] → [acceptance skill].
Filled in by each agent's showcase contribution.

| Subsystem | Feature | Agent | Exercised by | Audit skill |
|---|---|---|---|---|
| Nadir | Weighted scoring | game-ai-engineer | scout / brute / ranger archetypes | `/inspect-scoring` |
| Nadir | Action sequences | game-ai-engineer | patrol.actions.xml | `/replay-step` |
| Nadir | Hot-tunable weights | game-ai-engineer | aggression slider on brute | `/tune-weights` |
| Animation | Skeleton + IK | game-asset-engineer | humanoid enemies | visual inspection |
| Animation | Clip blending | game-asset-engineer | idle↔walk crossfade | `/waveform` (audio sync) |
| Scripts | Runtime attach | game-engine-architect | PatrolScript on scout | live attach in console |
| Physics | Collision | game-engine-architect | projectile + ground | `/barrier-audit` |
| Physics | Rigid body | game-engine-architect | falling crates (phase 4) | `/physics-step` |
| Rendering | Bindless descriptors | game-engine-architect | multi-material scene | `/descriptor-dump` |
| Rendering | Offscreen → ImGui | game-engine-architect | editor viewport | `/barrier-audit` |
| Lighting | Directional + point + spot | lighting-mood-architect | sun + torches + flashlight | `/kelvin-preview`, `/vibe-audit` |
| Lighting | Flicker + Kelvin | lighting-mood-architect | torch flicker curve | `/light-flicker-tune` |
| Lighting | Scene mood profile | lighting-mood-architect | Liminal preset active | `/mood-apply` |
| Lighting | Post-FX chain | lighting-mood-architect | CRT + EVA HUD + fog | `/postfx-add` |
| Audio | First-principles mixer | marty-odonnell-composer | 4 busses active | `/mixer-dump` |
| Audio | MusicDirector states | marty-odonnell-composer | explore ↔ combat ↔ victory | `/music-theme-set` |
| Audio | Stingers + leitmotifs | marty-odonnell-composer | boss-sighted stinger | `/stinger-fire` |
| Audio | Nadir sound requests | marty-odonnell-composer | enemy death SFX | `/sound-request-test` |
| Networking | UDP server/client | netcode-engineer | host + loopback client | `/server-start`, `/net-stats-dump` |
| Networking | Snapshot interp | netcode-engineer | networked enemies | `/snapshot-inspect` |
| Networking | Loss/latency | netcode-engineer | injector at 10% loss | `/inject-loss` |
| Networking | Replay determinism | netcode-engineer | 60s recording | `/replay-record`, `/replay-play` |
| Scripts | C++ scripts | game-engine-architect | PatrolScript, RespawnScript | direct invocation |
| Editor | Mode switching | game-engine-architect | Play↔Edit↔Simulate | `/mode-switch` |
| Editor | Hierarchy/Inspector | game-asset-engineer | every component editable | manual walkthrough |
| Editor | Scene round-trip | game-asset-engineer | save preserves comments | `/roundtrip-test` |
| Vibe | Charter adherence | vibe-story-guardian | whole scene | `/vibe-audit`, `/anti-touchstone-check` |

## Agents authoring their contributions
Per-agent design docs live in `demo/showcase/design/<agent-slug>.md`. The agent writes:
1. What entity/file/hookup exercises their domain.
2. What "working" looks like.
3. The exact acceptance skill invocation to run.
4. What the pass output looks like.
5. What a failure looks like (so regressions are catchable).

## Acceptance run
1. Build the showcase scene.
2. For each row in the matrix, the responsible agent runs its audit skill.
3. Results aggregated into `demo/showcase/acceptance_report.md` per run.
4. Below 100% pass → council convened to triage before any release.
