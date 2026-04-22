# /add-camera

Attach a CameraComponent to an entity. Renderer uses the entity flagged `is_main=true` in Play/Simulate modes.

## Usage
`/add-camera <entity-id> [--fov 70] [--near 0.1] [--far 1000] [--is-main false]`

## Fields
- `fov` — vertical field of view in degrees.
- `near` / `far` — clip-plane distances (meters).
- `is_main` — exactly ONE camera per scene should be main. Later rebinding auto-clears others.

## Behavior
- Renderer queries `is_main=true` camera every Play frame; falls back to editor orbit in Edit mode.
- CameraComponent is **client-local always** (`kReplicated = false`). Never crosses the wire.

## Typical pattern — camera as child of player
```xml
<entity id="player_camera" parent="player">
  <transform position="0 0.85 0"/>  <!-- eye at head level, pivot at feet -->
  <camera fov="70" is_main="true"/>
</entity>
```

## Side effects
- `EntityComponents::camera` set.
- If `is_main=true`, other cameras' `is_main` cleared.

## Invokable by
- Editor UI: Inspector → Add Component → Camera.
- Agents: `/add-camera <id>`.

## See also
- `/set-main-camera`, `/reparent-entity`, `/first-person-walker`.
