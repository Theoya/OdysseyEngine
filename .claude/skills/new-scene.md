# /new-scene

Create a new empty scene in the editor, discarding the current one (with dirty-check prompt).

## Usage
`/new-scene`

## Behavior
1. If `SceneData::mutated`, prompts Save / Discard / Cancel.
2. On Save: invokes `/save-scene` first.
3. Clears `EntityManager`, resets `SceneData`, empties `state_.scene_path`.

## Side effects
- All entities destroyed (selection cleared).
- Title bar shows "Untitled".

## Invokable by
- Editor UI: File → New Scene
- Keybind: Ctrl+N

## See also
- `/open-scene`, `/save-scene`
