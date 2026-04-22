# /frame-selected

Frame the selected entity in the viewport — move camera to focus on it.

## Usage

Press **F** (when scene viewport has focus) to frame the currently selected entity.

## Arguments

None — operates on `EditorState::selected_entity`.

## Behavior

Computes a camera pose that frames the entity:
- Positions camera 3× entity_radius away from the entity center
- Points toward entity with pitch = -15° (slight upward tilt for cinematic feel)
- Uses entity's bounding-box radius (default 1.0 if no collider present)

Updates `EditorState::viewport_camera` position, yaw, and pitch in place. Does not change FOV.

## Side effects

Modifies viewport camera state immediately. Frame-selected is an in-editor navigation gesture with no persistent effect.

## See also

`compute_frame_target()`, `/fly-camera`, `src/editor/scene_camera.h`
