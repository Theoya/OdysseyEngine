# /delete-entity

Remove an entity from the active scene via pure helper `editor::delete_entity`.

## Usage
`/delete-entity <entity-id>`

## Behavior
1. Looks up entity; errors if not found.
2. `EntityManager::destroy_entity(id)`.
3. Clears selection if the deleted entity was selected.
4. Marks scene dirty — reconstruct path on next save omits the entity.

## Returns
`Result<bool, std::string>` — true on success.

## Side effects
- EntityManager loses one entity.
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Hierarchy right-click → Delete
- Keybind: Del

## See also
- `/duplicate-entity`, `/add-entity` · Undo history lands in Batch F.
