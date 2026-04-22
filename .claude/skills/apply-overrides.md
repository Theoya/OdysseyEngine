# apply-overrides

**Apply Prefab Overrides (stub)** (Batch H).

## Summary
Stub implementation for persisting instance-level edits back to the prefab source. Full implementation deferred to Batch I / Phase 9.

## Capabilities
- **Batch H stub**: Returns `Result::err("Prefab overrides not yet implemented in Batch H")`
- **Batch I preview**: Will capture all component changes made to a prefab instance and write them back to the .prefab.xml source
- **Intent**: Allow non-destructive edits per instance while keeping the prefab definition as the baseline

## Typical Workflow (Batch I)
1. Right-click a prefab instance in the Hierarchy
2. Edit properties in the Inspector (health, color, etc.)
3. Right-click → "Apply Overrides"
4. Changes persist to the .prefab.xml file
5. All other instances still use the original source (unless they also override)

## Batch H Behavior
- Context menu "Apply Overrides" → logs "not yet implemented" + does nothing
- `apply_prefab_overrides(em, id)` returns error with "not yet implemented" message
- Stub allows menu to be visible but non-functional

## Implementation Details
- Function: `apply_prefab_overrides(em, id)` → `Result<bool, string>`
- Currently returns: `Result::err("Prefab overrides not yet implemented in Batch H")`
- Full impl will:
  - Locate the entity's prefab_source (path to .prefab.xml)
  - Load the prefab XML
  - Diff current entity state vs prefab baseline
  - Serialize differences as `<override>` children in the instance
  - OR write overrides to a separate .overrides.xml companion file

## Files
- `src/editor/prefab_ops.h` — public stub API
- `src/editor/prefab_ops.cpp` — stub implementation
- `src/editor/scene_tree_panel.cpp` — context menu entry
- `tests/unit/test_prefab_ops.cpp` — test_ApplyOverridesStubReturnsErr

## Notes
- This is a utility for power users; most workflows will just pack overrides into the instance
- Design decision (Batch I): whether overrides live in-scene or in a companion file TBD
- Revert Overrides (see `revert-overrides` skill) is the companion operation
