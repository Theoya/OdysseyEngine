# /status-bar

View the editor status bar at the bottom of the viewport.

## Usage
`/status-bar`

## Description
The status bar is a fixed-height, non-dockable window pinned to the bottom of the main viewport. It displays:

**Left side:**
- **Scene path**: Absolute path to the currently loaded scene, or "No scene" in red if none loaded
- **Entity count**: Total number of entities in the scene

**Center:**
- **Selection**: Name + ID of the selected entity, or "none" if no selection

**Right side:**
- **FPS**: Exponential moving average of frame time (smoothing α=0.1)
- **GPU memory**: VMA memory stats (if available; "GPU: ?" if unavailable)
- **Mode pill**: Colored chip showing current mode (EDIT=green, PLAY=red, SIM=blue)

The status bar is drawn automatically every frame after all panels are drawn. No user interaction; for reference only.

## Steps

### 1. Run the editor
Launch the editor executable. The status bar appears at the bottom.

### 2. Load a scene
Open a scene file (File → Open Scene). The status bar shows the scene path and entity count.

### 3. Select entities
Click on entities in the Viewport or Scene Tree. The status bar updates to show the selected entity's name and ID.

### 4. Monitor FPS
Watch the FPS reading (right side) to diagnose performance. The EMA smooths spikes.

### 5. Check mode
The mode pill (far right) confirms which mode you're in: Edit for manual editing, Play/Sim for simulation.

## Related
- `/build-settings` — configuration panel
- `/preferences` — editor preferences (font, snap, etc.)
