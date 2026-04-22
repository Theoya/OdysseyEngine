# /build-settings

Open the Build Settings panel in the editor.

## Usage
`/build-settings`

## Description
Opens the Build Settings dockable panel, which shows:
- **Target game**: Radio buttons to choose odyssey_shooter, odyssey_fps, or odyssey_editor
- **Scenes In Build**: List of scene XML files to include in the binary distribution, with drag-reorder and add/remove buttons
- **Configuration**: Dropdown to select Debug, Release, or RelWithDebInfo
- **Output Dir**: Path to the distribution folder (read-only, with "Open Output Folder" button)
- **Build button**: Async starts `cmake --build <build_dir> --config <cfg> --target <target>`, showing spinner and partial stdout

The panel persists window-docked state via ImGui docking, and scenes list is always pre-populated from editor.xml or a saved build config file.

## Steps

### 1. Panel already created
The Build Settings panel is created at editor startup and added to the Window → View menu.

### 2. Click to show
If the panel is hidden, select it in **View** menu or this skill will show it automatically.

### 3. Configure and build
- Select target (shooter, fps, or editor)
- Add or remove scenes from the list (click "Add Scene" for file dialog)
- Select config (Release recommended for distributions)
- Adjust output directory if needed
- Click "Build" button

### 4. Monitor
The panel shows a spinner while the build is running, and logs full cmake output in a collapsing header below the build button.

## Related
- `/build-game` — actually starts the build (equivalent to clicking the button)
- `/open-output-folder` — directly opens the distribution folder in Explorer
