# /reparent-entity

Reparent an entity in the scene graph. Child transforms compose as `world = parent.world × local`. Cycle and self-parent are rejected.

## Usage
`/reparent-entity <child-id> <parent-id>` · `/reparent-entity <child-id> none` to unparent.

## Behavior
1. Validate no cycle (walk parent chain, depth cap 64). Err on `Cycle`, `SelfParent`, `UnknownParent`, `DepthExceeded`.
2. `Entity::parent_id = new_parent_id` (or `INVALID_ENTITY` to unparent).
3. Mark SceneData mutated; next `compose_world_transforms()` re-sorts topologically.

## Returns
`Result<void, HierarchyError>`.

## Side effects
- `Entity::parent_id` updated.
- `SceneData::mutated = true`.
- Cached topological sort invalidated.

## Invokable by
- Editor UI: Hierarchy drag-drop (entity onto another row).
- Agents: `/reparent-entity <child> <parent>`.

## See also
- `/create-entity`, `/delete-entity`.
