# /add-collider

Attach a collider shape to an entity: Box / Sphere / Capsule / Mesh.

## Usage
- `/add-collider <entity-id> box --half-extents x y z`
- `/add-collider <entity-id> sphere --radius r`
- `/add-collider <entity-id> capsule --radius r --height h` (h must be ≥ 2r; axis = Y)
- `/add-collider <entity-id> mesh --path demo/.../thing.mesh.xml`

## Behavior
Populates the matching `std::optional<>` on `EntityComponents`:
- `box_collider`, `sphere_collider`, `capsule_collider`, `mesh_collider`.
Each kind is mutually exclusive per entity (attach one; re-attaching swaps).

## Contact resolution
- Box ↔ Sphere ↔ Capsule vs ground plane: supported.
- Mesh collider uses Möller–Trumbore ray-triangle under the hood.
- Restitution defaults to 0 (no bounce); friction 0.5 Coulomb.

## Side effects
- Collider element serialized into the entity's scene XML.
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Inspector → Add Component → <Kind>Collider.
- Agents: `/add-collider ...`.

## See also
- `/add-rigidbody`, `/physics-step`, `/integrator-test`.
