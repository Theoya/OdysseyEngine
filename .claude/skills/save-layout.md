# save-layout

Save the current editor window layout as a named preset.

## Signature
```
/save-layout [name]
```

## Description
Exports the current ImGui docking layout to `<exe_dir>/layouts/<slug_name>.ini`, creating a reusable preset.

## Arguments
- `name` — Human-readable layout name (e.g., "Tall Inspector", "2x3 Grid"). Spaces and punctuation are stripped; max 64 chars.

## Behavior
- Generates a slug from the name via `slug_layout_name()`.
- Calls `ImGui::SaveIniSettingsToDisk()` to dump the current layout to `<exe_dir>/layouts/<slug>.ini`.
- Persists the active layout name in `editor_prefs.xml` via `editor_prefs.active_layout`.

## Implementation Location
- Menu: `Window → Layout → Save Current As…` in `src/editor/editor.cpp::draw_menu_bar()`
- Slug generation: `src/editor/layout_presets.cpp::slug_layout_name()`
