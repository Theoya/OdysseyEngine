# /add-component

Add a Unity-style component to the selected entity (Transform / Stats / MeshRenderer / Behavior / Script / Tags / VoiceSource).

## Usage
`/add-component <kind>` — `kind` ∈ {Transform, Stats, MeshRenderer, Behavior, Script, Tags, VoiceSource}

## Behavior
Uses `editor::all_component_descriptors()` table. The picker shows only components currently ABSENT on the entity (see `missing_components`). Clicking invokes that kind's `add` lambda which sets default field values and marks `SceneData::mutated`.

## Side effects
- `EntityComponents` field(s) initialized to defaults for the chosen kind.
- Inspector immediately renders the new card.

## Invokable by
- Editor UI: Inspector → "➕ Add Component" button (bottom)

## See also
- `/remove-component`, `/reset-component`, `/copy-component-values`, `/paste-component-values`
