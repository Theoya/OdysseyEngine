# /remove-component

Remove a component from the selected entity by zeroing its fields.

## Usage
`/remove-component <kind>` — `kind` ∈ {Stats, MeshRenderer, Behavior, Script, Tags, VoiceSource}

Transform is NOT removable (mandatory).

## Behavior
Invokes the descriptor's `remove` lambda. For path-based components (MeshRenderer, Behavior, Script) this clears the path/class strings. For Tags it empties the vector. For VoiceSource it sets `voice_range=0`.

## Side effects
- `EntityComponents` field(s) zeroed.
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Inspector card → `…` hamburger → Remove

## See also
- `/add-component`, `/reset-component`
