# /step-physics

Advance PhysicsWorld by exactly one fixed substep (1/60 s).

## Usage
`/step-physics` (advances one substep) · `/step-physics --n 5` (advances N substeps).

## Behavior
Calls `Engine::physics_world_mut().step(1.0/60.0)` from the CLI / debug console. Only permitted in Edit or paused Play mode. Returns after body positions are updated and contacts resolved.

## Tick order context
`Engine::process_frame` runs: Input → Script::pre_physics → PhysicsWorld::step (fixed-dt accumulator) → Script::post_physics → compose_world_transforms → Nadir → Render → Audio.

## Side effects
- Rigidbody positions/velocities updated.
- Contact solver writes collision impulses.
- EntityComponents::transform updated on next `compose_world_transforms`.

## Invokable by
- CLI: `odyssey.exe step-physics`.
- Debug panel: Physics Debug → Step Once.
- Agents: `/step-physics`.

## See also
- `/integrator-test`, `/physics-step`, `/first-person-walker`.
