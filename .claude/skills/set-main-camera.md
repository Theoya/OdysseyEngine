# /set-main-camera

Set the main camera for the active scene. Exactly one camera is main at any time.

## Usage
`/set-main-camera <entity-id>`

## Behavior
1. Clears `is_main` on all other CameraComponents in the scene.
2. Sets `is_main=true` on the target entity's camera.
3. Renderer picks it up next frame in Play/Simulate modes.

## Returns
`Result<void, std::string>` — Err if entity has no CameraComponent.

## Side effects
- `EntityComponents::camera::is_main` toggled on all cameras.
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Viewport toolbar camera dropdown.
- Agents: `/set-main-camera <id>`.

## See also
- `/add-camera`, `/first-person-walker`.
