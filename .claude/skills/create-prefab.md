# create-prefab

**Hierarchy right-click → Create Prefab** (Batch H).

## Summary
Captures an entity's components into a `.prefab.xml` file and replaces the entity with a prefab instance.

## Capabilities
- Writes entity state to a new `.prefab.xml` file (pugixml)
- Copies transform (position, rotation, scale) and stats (health, max_health)
- Replaces original entity with a prefab instance reference
- Generates safe filenames from entity names

## Typical Workflow
1. Right-click an entity in the Hierarchy
2. Select "Create Prefab"
3. A new `.prefab.xml` file is written to the prefabs directory
4. The original entity is replaced with an instance pointing to the prefab
5. Log confirms the prefab was created

## Example Output
```xml
<?xml version="1.0"?>
<prefab>
  <entity id="42" name="my_enemy" archetype="goblin">
    <transform position="1.5 2.0 3.5" rotation="0 0 0 1" scale="1 1 1"/>
    <stats health="100" max_health="100"/>
  </entity>
</prefab>
```

## Implementation Details
- Function: `create_prefab_from_entity(em, id, prefabs_dir)` → `Result<filesystem::path, string>`
- Validates EntityID exists
- Generates filename from entity.name (sanitize spaces → underscores)
- Writes XML via pugixml
- Returns path on success or error message on failure
- Returns `Result<path, string>::err()` if entity not found or file write fails

## Files
- `src/editor/prefab_ops.h` — public API
- `src/editor/prefab_ops.cpp` — pugixml implementation
- `src/editor/scene_tree_panel.cpp` — context menu "Create Prefab" entry
- `tests/unit/test_prefab_ops.cpp` — test_CreatePrefabWritesValidXML, test_CreatePrefabInvalidEntityReturnsErr

## Notes
- Prefab files use `.prefab.xml` extension (mirrors scene.xml naming)
- Full "Edit Prefab" (isolation mode) deferred to Batch I / Phase 9
- Unpack is a separate operation (see `unpack-prefab` skill)
