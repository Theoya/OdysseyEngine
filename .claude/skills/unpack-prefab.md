# unpack-prefab

**Unpack Prefab → Replace instance with constituents** (Batch H).

## Summary
Replace a prefab instance entity with its constituent child entities, effectively "exploding" the prefab. Batch H provides a minimal implementation; full unpacking (re-instantiate all children from prefab source) deferred to Batch I.

## Capabilities
- **Batch H**: Deletes the prefab instance entity
- **Batch I preview**: Will load the prefab XML, re-create all child entities in the scene, delete the instance
- **Intent**: Break a prefab instance into independent entities for customization

## Typical Workflow (Batch I)
1. Place a "house" prefab in scene (contains door, roof, walls entities)
2. Right-click → "Unpack Prefab"
3. Instance deleted; door, roof, walls now appear as separate scene entities
4. Can now edit them individually (different colors, positions, etc.)
5. Changes do NOT affect the prefab source or other instances

## Batch H Behavior
- Context menu "Unpack Prefab" → simply deletes the instance
- `unpack_prefab(em, id)` removes the entity from EntityManager
- Logs: "Unpacked prefab instance X"
- No children re-created (will be added in Batch I)

## Implementation Details
- Function: `unpack_prefab(em, id)` → `Result<bool, string>`
- Current impl: validates entity exists, calls `em.destroy_entity(id)`, returns `ok(true)`
- Full impl will:
  - Load entity.prefab_source (path to .prefab.xml)
  - Parse prefab XML for child definitions
  - Instantiate each child as a new entity in the scene
  - Restore spatial + component relationships
  - Delete the instance

## Files
- `src/editor/prefab_ops.h` — public API
- `src/editor/prefab_ops.cpp` — simple implementation
- `src/editor/scene_tree_panel.cpp` — context menu entry
- `tests/unit/test_prefab_ops.cpp` — test_UnpackPrefabRemovesInstance, test_UnpackPrefabInvalidEntityReturnsErr

## Notes
- This is reversible: re-create the prefab instance manually, or use an asset library
- Batch I version will respect the prefab's original hierarchy (parent/child relationships)
- Unpacking is useful for one-off customization without polluting the prefab source
