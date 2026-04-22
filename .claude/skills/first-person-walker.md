# /first-person-walker

Overview of the walker demo scene at `demo/showcase/walker.scene.xml`.

## What it demonstrates
- Entity parenting: `player_camera` is a child of `player`, so the camera follows the body (position composes via `world = parent.world × local`).
- Physics: player has CapsuleCollider r=0.4 h=1.8 and Rigidbody m=75kg useGravity=true. Gravity (−9.81 m/s²) pulls to the ground plane.
- Fixed-dt physics: `Engine::process_frame` runs `PhysicsWorld::step(1/60 s)` substeps per frame.
- Mood-driven lighting: scene binds `walker_testbed` lighting_profile (5600K sun + 5000K ambient).
- FirstPersonController script: WASD + mouse-look + gravity + ground-ray check.

## Tuning constants (FirstPersonController)
| Constant | Value | Intent |
|---|---|---|
| `kWalkSpeed` | 4.0 m/s | brisk-not-sprinting; "body in a world" |
| `kJumpImpulse` | 5.5 m/s | ~1.5 m under Earth gravity |
| `kGravity` | 9.81 m/s² | Earth standard; no fudge |
| `kEyeHeight` | 1.7 m | head clearance below capsule top |
| `kMouseSensitivity` | 0.002 rad/px | Fantasy-Etherealism weighted |
| `kHeadBobAmplitude` | 0.02 m | subtle; not arcade |
| `kHeadBobFrequency` | 2.0 Hz | at kWalkSpeed |

## Audio hook (Phase 10+)
Ground-contact transitions will fire `footstep_event { surface_type, velocity, entity_id }` via a future SfxDirector subsystem.

## Run
`odyssey_editor.exe demo/showcase/walker.scene.xml` → press Play → walk.

## Out of scope (deferred)
- Shadow map, terrain, GLTF loader — separate council votes.
- Net replication — Phase 11+.
