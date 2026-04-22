# /save-scene

Save the active scene back to its original path via `scene::serialize_scene`.

## Usage
`/save-scene`

## Behavior
Requires `Mode::Edit`. Writes `SceneData` to `state_.scene_path`. Echoes `preserved_source` byte-identically when `mutated=false`; reconstructs XML (preserving unknowns) when `mutated=true`. Re-snapshots file into `preserved_source` after write.

## Side effects
- File written.
- `SceneData::mutated` cleared.
- Title bar dirty-indicator cleared.

## Invokable by
- Editor UI: File → Save
- Keybind: Ctrl+S

## See also
- `/save-scene-as`, `/open-scene`
