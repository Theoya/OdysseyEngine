# /set-camera-speed

Programmatically set the scene camera base movement speed.

## Usage
`/set-camera-speed <speed>`

## Arguments
- `<speed>`: Movement speed in meters/second (1–100 recommended, default 10)

## Description
Updates the `scene_camera_base_speed` preference, which affects free-fly camera movement in Edit mode.

The camera uses this speed when WASD keys are pressed. No UI interaction required; changes take effect immediately.

## Steps

### 1. Ensure Preferences panel is visible
If hidden, use `/preferences` to open it.

### 2. Set speed programmatically
This skill updates `Preferences::scene_camera_base_speed = speed` and applies it to the SceneCamera instance.

### 3. Verify
Move the camera in the viewport (WASD + right-click to look) to feel the new speed. Slower speeds (e.g., 5.0) are better for detail work; faster speeds (e.g., 30.0) for exploring large scenes.

### 4. Persist
Manually click Apply in the Preferences panel to save to disk, or changes are lost on editor restart.

## Related
- `/preferences` — open the Preferences panel to manually adjust and apply camera speed
- `/set-font-size` — adjust editor font size
