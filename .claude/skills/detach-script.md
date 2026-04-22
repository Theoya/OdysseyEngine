# /detach-script

Detach the attached Script from the selected entity.

## Usage
`/detach-script`

## Behavior
Clears `EntityComponents::script_class` and `script_config`. The Script card collapses to the "+ Attach Script" prompt.

## Side effects
- `EntityComponents::script_class = ""`
- `EntityComponents::script_config = ""`
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Inspector Script card → `…` hamburger → Detach.

## See also
- `/attach-script`
