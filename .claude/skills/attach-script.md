# /attach-script

Attach a registered C++ Script class to the selected entity.

## Usage
`/attach-script <class-name>`

## Behavior
1. Looks up `class-name` in `ScriptRegistry::list_registered_script_classes()`. Errors if not registered.
2. Sets `EntityComponents::script_class = class_name` and `script_config = ""`.
3. Inspector immediately renders the Script card with a textarea for `script_config` (raw XML blob).

## Side effects
- `EntityComponents::script_class` set.
- `SceneData::mutated = true`.

## Invokable by
- Editor UI: Inspector Script card → "+ Attach Script" → picker.

## See also
- `/detach-script` · Compile-on-save (Unity-style) is out of scope.
