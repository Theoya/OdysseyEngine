# /toggle-snap

Toggle grid snap on/off in the viewport.

## Usage

In the Edit menu or viewport toolbar, click "Snap" to toggle snap-to-grid constraint.

## Arguments

None — operates on `EditorState::snap_enabled`.

## Behavior

When snap is ON:
- Gizmo transformations snap to increment values (position, rotation, scale)
- ImGuizmo `SetSnap` constraint is applied with increments from `EditorState`
- Visual feedback: grid overlay shows snap cells (Batch D—simple Y=0 plane with 1m cells)

When snap is OFF:
- Free-form transformation with no constraint
- Grid overlay may still be visible (controlled by `show_grid`)

Snap settings (position, rotation, scale increments) are persisted in `editor_prefs.xml` (Batch G).

## Side effects

Modifies `EditorState::snap_enabled`. Affects gizmo behavior in next frame. No I/O per-frame.

## See also

`/set-snap-increment`, `/toggle-snap` variant names, src/editor/editor.h (snap_enabled field)
