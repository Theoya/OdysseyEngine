# /fly-camera

Document the free-fly scene camera controls in the Unity-class editor viewport.

## Usage

In Edit mode, the scene viewport camera is free-fly (not auto-orbit). Use these controls to navigate:

## Controls

- **Right-mouse hold + drag:** Rotate camera (pitch/yaw). Sensitivity: 0.005 rad/pixel.
- **WASD:** Move along camera basis (W=forward, S=backward, A=left, D=right).
- **Q/E:** Move up/down (world Y-axis).
- **Shift:** 3× speed multiplier (base speed 10 m/s).
- **Scroll wheel:** Dolly along forward direction (5 m/notch).

## Behavior

All movement is frame-rate independent (uses delta_time). Pitch clamps to ±89° to avoid gimbal lock. Camera position and orientation are stored in `EditorState::viewport_camera`.

In Play/Simulate modes, the camera reverts to auto-orbit to preserve scene cinematic feel.

## Side effects

Modifies `EditorState::viewport_camera` each frame in Edit mode. No file I/O or network traffic.

## See also

`scene_camera.h`, `/frame-selected`, `/transform-tool`
