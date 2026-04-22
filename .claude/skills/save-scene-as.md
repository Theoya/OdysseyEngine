# /save-scene-as

Save the active scene to a user-chosen new path via Win32 save-dialog.

## Usage
`/save-scene-as [path]`

## Arguments
- `path` (optional): absolute path. If omitted, shows `GetSaveFileNameW` (default ext `.scene.xml`, overwrite-prompt enabled).

## Behavior
Serializes to the chosen path, updates `state_.scene_path`, prepends to recent, clears dirty.

## Side effects
- New file written.
- `state_.scene_path` changed.
- Recent list updated.

## Invokable by
- Editor UI: File → Save As…
- Keybind: Ctrl+Shift+S

## See also
- `/save-scene`, `/open-scene`
