# reset-layout

Reset the editor layout to the built-in Default preset.

## Signature
```
/reset-layout
```

## Description
Discards any custom layout and restores the default editor layout (20% Hierarchy left, 25% Inspector right, 25% Panels bottom, center Viewport).

## Behavior
- Calls `preset_splits(LayoutPreset::Default)` to retrieve default split instructions.
- Rebuilds the ImGui dock layout from scratch via DockBuilder.
- Sets `editor_prefs.active_layout = "Default"`.
- Saves preferences to disk.

## Implementation Location
- Menu: `Window → Layout → Reset to Default` in `src/editor/editor.cpp::draw_menu_bar()`
- Default preset: `src/editor/layout_presets.cpp::preset_splits(LayoutPreset::Default)`
