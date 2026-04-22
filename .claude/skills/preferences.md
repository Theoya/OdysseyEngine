# /preferences

Open the Preferences panel in the editor (Edit menu).

## Usage
`/preferences`

## Description
Opens the Preferences dockable panel from the Edit menu, which shows:
- **Editor Font Size**: DragFloat to set base ImGui font scale (default: 14.0)
- **Scene Camera Base Speed**: DragFloat for free-fly viewport speed in Edit mode (default: 10.0 m/s)
- **Transform Snap**: DragFloats for position snap, rotation snap (degrees), and scale snap
- **Autosave**: DragInt to set autosave interval in seconds (0 = off)
- **Theme**: Checkbox for dark/light theme toggle (default: dark)
- **Apply button**: Saves to disk (`editor_preferences.xml`) and applies changes live

All changes are automatically persisted to `<exe_dir>/editor_preferences.xml` when Apply is clicked.

## Steps

### 1. Open Preferences
Click **Edit → Preferences** in the menu bar, or this skill will open it automatically.

### 2. Adjust settings
- Drag sliders to tune font size, camera speed, snap values, and autosave interval
- Toggle theme checkbox
- Values update in real-time in the UI

### 3. Apply
Click **Apply** button to:
- Save all settings to `editor_preferences.xml`
- Apply ImGui font scale immediately (via `ImGui::GetIO().FontGlobalScale`)
- Update EditorState snap settings
- Log success/failure to console

### 4. Verify
Settings persist across editor sessions. Close and reopen the editor to confirm.

## Related
- `/set-font-size` — programmatically set editor font scale
- `/set-camera-speed` — programmatically set scene-camera base speed
