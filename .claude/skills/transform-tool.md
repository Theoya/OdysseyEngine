# /transform-tool

Switch gizmo operation mode (translate, rotate, scale, select).

## Usage

Press a hotkey in the viewport to change the active transform mode:

- **Q** → Select mode (gizmo hidden)
- **W** → Translate mode (move entity)
- **E** → Rotate mode (rotate entity)
- **R** → Scale mode (scale entity)
- **T** → Universal mode (combined transform)

## Arguments

None — hotkeys operate on `EditorState::gizmo_mode`.

## Behavior

Sets `EditorState::gizmo_mode` to one of:
- `GizmoMode::Select`
- `GizmoMode::Translate`
- `GizmoMode::Rotate`
- `GizmoMode::Scale`
- `GizmoMode::Universal`

Only responds when viewport has keyboard focus (not typing in text fields). Default mode is `Translate`.

## Side effects

Changes editor state (no persistent effect). ImGuizmo renders the appropriate widget in the next frame.

## See also

`/toggle-gizmo-space`, `src/editor/editor.h` (GizmoMode enum), ImGuizmo documentation
