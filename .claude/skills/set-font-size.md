# /set-font-size

Programmatically set the editor font size.

## Usage
`/set-font-size <size>`

## Arguments
- `<size>`: Font size in points (8–32 recommended, default 14)

## Description
Directly updates the `editor_font_size` preference and applies it live to ImGui by setting `ImGui::GetIO().FontGlobalScale = size / 14.0f`.

No UI interaction required; changes take effect immediately in the next frame without recompiling fonts.

## Steps

### 1. Ensure Preferences panel is visible
If hidden, use `/preferences` to open it.

### 2. Set size programmatically
This skill calls `Preferences::editor_font_size = size` and invokes `apply_live_changes(state)`, which updates the global ImGui font scale.

### 3. Verify
The editor UI text size changes on the next frame. Persist the change by calling Apply in the Preferences panel manually, or the change is lost on editor restart.

## Related
- `/preferences` — open the Preferences panel to manually adjust font size and apply/save
- `/set-camera-speed` — adjust viewport camera movement speed
