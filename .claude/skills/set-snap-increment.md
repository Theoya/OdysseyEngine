# /set-snap-increment

Configure snap increment values for position, rotation, and scale.

## Usage

Access in Preferences (Batch G) or via in-code calls to modify:
- `EditorState::snap_position` (default 0.25 m)
- `EditorState::snap_rotation` (default 15°)
- `EditorState::snap_scale` (default 0.1 units)

## Arguments

When called programmatically:
- `position`: float, meters (e.g., 0.5, 1.0)
- `rotation`: float, degrees (e.g., 5.0, 15.0, 45.0)
- `scale`: float, scale units (e.g., 0.05, 0.1, 0.25)

## Behavior

When snap is enabled (`snap_enabled == true`), gizmo transformations snap to these increments:
- Position: quantized to `snap_position` m intervals
- Rotation: quantized to `snap_rotation` degree intervals
- Scale: quantized to `snap_scale` unit intervals

Values are stored in `EditorState` and persisted to `editor_prefs.xml` on editor shutdown (Batch G).

## Side effects

Updates `EditorState` fields. In next frame, ImGuizmo applies constraints via `SetSnap(snap_vec)`.

## See also

`/toggle-snap`, `src/editor/editor.h`, editor_prefs.xml schema (Batch G)
