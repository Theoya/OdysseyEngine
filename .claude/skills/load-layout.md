# load-layout

Apply a saved editor layout preset.

## Signature
```
/load-layout [preset_name]
```

## Description
Loads a previously-saved layout preset or applies a built-in layout. Built-in presets are "Default", "2-by-3", "Tall", and "Wide". User-defined layouts are stored in `<exe_dir>/layouts/*.ini`.

## Arguments
- `preset_name` — Name of the layout (built-in or saved). Case-insensitive.

## Behavior
- If the name matches a built-in preset, calls `preset_splits()` and rebuilds the dock layout via ImGui::DockBuilder.
- If a user layout file exists at `<exe_dir>/layouts/<slug_name>.ini`, calls `ImGui::LoadIniSettingsFromDisk()`.
- Updates `editor_prefs.active_layout` to reflect the new layout.

## Implementation Location
- Menu: `Window → Layout → [Preset Options]` or `Window → Layout → User Layouts` in `src/editor/editor.cpp::draw_menu_bar()`
- Preset lookup: `src/editor/layout_presets.cpp::preset_from_name()`
