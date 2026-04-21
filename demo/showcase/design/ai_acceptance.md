# Showcase AI Acceptance

Criteria for PASS/FAIL on the AI layer of `demo/showcase/showcase.scene.xml`.
All referenced behavior shaders live in `demo/behaviors/` (shared with the
shooter demo) and are resolved by name via the Nadir directory scan.
Showcase-specific overrides, if any, go in `demo/showcase/behaviors/` and
win by search order — see `test_nadir_pipeline.cpp::NadirShowcaseCompile`.

## Archetypes under test

| scene archetype       | shader                     | role                                           |
|-----------------------|----------------------------|------------------------------------------------|
| `player`              | `player_input.nadir`       | pass-through; CPU script drives movement       |
| `enemy_pack_hunter`   | `enemy_pack_hunter.nadir`  | pack cohesion + flanking, state machine        |
| `enemy_ranged`        | `enemy_ranged.nadir`       | keep-distance sniper, strafe + reposition      |
| `multi_arm_gunner`    | `multi_arm_gunner.nadir`   | boss, 4 per-arm weapon scores, HP phases       |
| `civilian`            | `civilian_fleeing.nadir`   | flee nearest threat, scatter, wander when calm |

## `/inspect-scoring` gates

Run `/inspect-scoring <entity_id>` on one representative of each archetype.
PASS requires:

- **player_1** — `outputs.move_vector` matches CPU PlayerController input
  (shader is a pass-through from `persist.memory_0`); `action_request == 0`.
- **hunters_left[0]** — `persist.current_state` transitions
  `PATROL → ALERT → COMBAT` as the player enters 30u/20u/10u radii.
  In COMBAT, `attack_target.w` cycles with `cooldown_0` (1.5 s).
  `comms_signal > 0` the first tick of COMBAT.
- **snipers[0]** — dist-to-player stays in `[25, 40]` band; `attack_target.w`
  peaks inside a `bell_curve(0.65, 0.15)` window; no fire when
  `cooldown_0` not ready.
- **boss_gunner** — all four per-arm scores visible in the debug buffer
  (`debug_pack4`): shotgun at close, sniper at long + stationary, smg at
  medium, shield when `incoming_damage_norm` spikes. `action_priority=0.9`
  overload fires when 3+ weapon scores > 0.4.
- **civilians[0]** — `STATE_FLEE` triggered when `score_proximity(dist,25)`
  crosses 0.3; `STATE_IDLE` otherwise. Scream sound event (id 6) fires only
  in first 0.5 s of a fresh FLEE entry.

## `/replay-step` gates

Record 10 s of combat via `/replay-record`, then `/replay-step`:

- **Hysteresis** — no `current_state` oscillates more than once per second
  on any entity (hysteresis bonus should damp ping-pong).
- **Boss phase lock** — `multi_arm_gunner` enters COVER only when
  `incoming_damage_norm > 0.4` *and* `health_norm < 0.5`; exits COVER
  within 2 s after damage stops.
- **Pack cohesion** — for `hunters_left`, 6-of-8 members stay within 15 u
  of `ally_center` across 300 ticks.
- **Determinism** — same input replay produces identical persist ring
  buffers for 2 consecutive runs (modulo float timing jitter in `dt`).

## FAIL triggers

- Any shader fails to compile under the pipeline test suite.
- Any entity emits `move_vector` with `NaN` or length > speed * 1.1.
- Boss enters `action_request=4` (overload) more than once per 6 s.
- Civilians attack (`attack_target.w > 0`).
- Hunters call comms (`comms_signal > 0`) every frame instead of gated by
  `cooldown_1`.
