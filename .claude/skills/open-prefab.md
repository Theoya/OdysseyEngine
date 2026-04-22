# open-prefab

**Edit Prefab → Prefab isolation mode (stub)** (Batch H).

## Summary
Stub implementation for opening a prefab in isolation for editing. Full implementation deferred to Batch I / Phase 9.

## Capabilities
- **Batch H stub**: Returns `Result::err("Prefab isolation mode not yet implemented in Batch H")`
- **Batch I preview**: Will load a prefab into a temporary isolation session (separate EntityManager)
- **Modal popup**: "Prefab isolation mode lands in Phase 9" message shown when user selects "Edit Prefab" from context menu

## Typical Workflow (Batch I)
1. Right-click a prefab instance in the Hierarchy
2. Select "Edit Prefab"
3. Prefab opens in a special isolation window
4. User edits components + saves
5. All instances of that prefab update
6. Exit isolation mode

## Batch H Behavior
- Context menu "Edit Prefab" → popup: "Prefab isolation mode lands in Phase 9"
- `open_prefab_in_isolation()` returns error with "not yet implemented" message
- Log message indicates deferred functionality

## Implementation Details
- Function: `open_prefab_in_isolation(prefab_path)` → `Result<bool, string>`
- Currently returns: `Result::err("Prefab isolation mode not yet implemented in Batch H")`
- Full impl will: load prefab XML, create temporary EntityManager, show modal editor
- Modal stub popup in scene_tree_panel.cpp with explanatory text + OK button

## Files
- `src/editor/prefab_ops.h` — public stub API
- `src/editor/prefab_ops.cpp` — stub implementation
- `src/editor/scene_tree_panel.cpp` — context menu entry + popup
- `tests/unit/test_prefab_ops.cpp` — test_OpenPrefabStubReturnsErr

## Notes
- This is a scoped stub to satisfy the context menu; real work deferred
- Will require a second temporary EntityManager to avoid mutating the main scene
- Batch I will also add persistence hooks (save changes back to .prefab.xml)
