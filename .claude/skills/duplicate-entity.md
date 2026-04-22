# /duplicate-entity

Clone an entity with all its components via pure helper `editor::duplicate_entity`.

## Usage
`/duplicate-entity <entity-id>`

## Behavior
1. Looks up source entity. Errors if not found.
2. Creates new entity with same archetype, name `"<original> (Copy)"`.
3. Copies all `EntityComponents` fields and `active` flag.
4. Selects the new entity.

## Returns
`Result<EntityID, std::string>` — new ID on success.

## Side effects
- EntityManager gains one entity.
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Hierarchy right-click → Duplicate
- Keybind: Ctrl+D

## See also
- `/add-entity`, `/delete-entity`
