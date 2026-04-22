# /add-rigidbody

Attach a Rigidbody component to an entity. Physics simulation begins next frame once a collider is also attached.

## Usage
`/add-rigidbody <entity-id> [--mass 75] [--drag 0.1] [--use-gravity true] [--kinematic false]`

## Behavior
1. `EntityComponents::rigidbody = physics::RigidBody{ mass, drag, use_gravity, is_kinematic }`.
2. On next Play-mode tick, `PhysicsWorld::step` integrates the body (semi-implicit Euler, fixed 1/60s substep).
3. If no collider is attached, body is a ghost (no contacts, still falls under gravity).

## Fields
- `mass` (kg, float > 0) — 0 disallowed; use `is_kinematic` for infinite-mass static.
- `drag` (per second, float ≥ 0) — linear damping.
- `use_gravity` (bool) — true enables world `PhysicsWorld.gravity`.
- `is_kinematic` (bool) — true means forces ignored; script writes position directly.

## Side effects
- `EntityComponents::rigidbody` set.
- Serializer round-trips `<rigidbody>` element.
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Inspector → Add Component → Rigidbody.
- Agents: `/add-rigidbody <id>`.

## See also
- `/add-collider`, `/step-physics`, `/integrator-test`.
