# /open-scene

Open a `.scene.xml` file in the editor via Win32 file-dialog.

## Usage
`/open-scene [path]`

## Arguments
- `path` (optional): absolute path. If omitted, shows `GetOpenFileNameW` filtered to `*.scene.xml`.

## Behavior
1. Dirty-prompt if needed.
2. Sets `state_.scene_swap_request`; main loop performs the swap next frame (no in-draw recursion).
3. Prepends path to the recent-scenes list (dedup, truncate to 8).

## Side effects
- `SceneData` + `EntityManager` replaced.
- `state_.scene_path` → absolute path.
- `editor_prefs.xml` updated.

## Invokable by
- Editor UI: File → Open Scene…
- Keybind: Ctrl+O

## See also
- `/new-scene`, `/save-scene`, `/save-scene-as`
