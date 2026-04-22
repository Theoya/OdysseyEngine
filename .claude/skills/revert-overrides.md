# revert-overrides

**Revert Prefab Overrides (stub)** (Batch H).

## Summary
Stub implementation for discarding all instance-level edits and reverting to the prefab source. Full implementation deferred to Batch I / Phase 9.

## Capabilities
- **Batch H stub**: Returns `Result::err("Prefab overrides not yet implemented in Batch H")`
- **Batch I preview**: Will reload the entity's components from the prefab source, discarding all local edits
- **Intent**: Allow users to abandon changes and return to the baseline prefab definition

## Typical Workflow (Batch I)
1. Right-click a prefab instance in the Hierarchy
2. Notice entity has been modified (health=50, was 100)
3. Right-click → "Revert Overrides"
4. Confirmation dialog: "Revert to prefab source? You will lose all changes."
5. Entity reloaded from .prefab.xml (health=100 again)

## Batch H Behavior
- Context menu "Revert Overrides" → logs "not yet implemented" + does nothing
- `revert_prefab_overrides(em, id)` returns error with "not yet implemented" message
- Stub allows menu to be visible but non-functional

## Implementation Details
- Function: `revert_prefab_overrides(em, id)` → `Result<bool, string>`
- Currently returns: `Result::err("Prefab overrides not yet implemented in Batch H")`
- Full impl will:
  - Validate entity has a valid prefab_source reference
  - Load prefab XML from disk
  - Re-instantiate the entity from prefab definition (overwriting current state)
  - Log: "Reverted entity X to prefab source Y"

## Files
- `src/editor/prefab_ops.h` — public stub API
- `src/editor/prefab_ops.cpp` — stub implementation
- `src/editor/scene_tree_panel.cpp` — context menu entry
- `tests/unit/test_prefab_ops.cpp` — test_RevertOverridesStubReturnsErr

## Notes
- Destructive operation: should show confirmation dialog before executing
- Companion to Apply Overrides (see `apply-overrides` skill)
- Will require safe error handling for missing/deleted prefab files
