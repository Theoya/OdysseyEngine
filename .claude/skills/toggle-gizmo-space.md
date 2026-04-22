# /toggle-gizmo-space

Toggle gizmo coordinate space between Local and World.

## Usage

Press **X** (when viewport has focus) to toggle the gizmo's coordinate frame.

## Arguments

None — operates on `EditorState::gizmo_space`.

## Behavior

Flips `gizmo_space` between:
- `GizmoSpace::Local` — gizmo axes align to entity's local rotation
- `GizmoSpace::World` — gizmo axes align to world axes (default)

When `gizmo_space == Local`, ImGuizmo constraint is set to local mode. Transformations respect entity orientation. When `World`, all axes are global (X, Y, Z).

Only responds when viewport has keyboard focus. Hotkey is active in any gizmo mode.

## Side effects

Changes `EditorState::gizmo_space`. Affects gizmo rendering and constraint behavior in the next frame.

## See also

`/transform-tool`, `src/editor/editor.h` (GizmoSpace enum), ImGuizmo `SetMode`
