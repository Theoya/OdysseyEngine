# /hot-reload

Modify a behavior shader and use the hot-reload workflow to see changes live.

## Usage
`/hot-reload <shader> [--describe]`

## Arguments
- `<shader>`: Name or path of the `.nadir` behavior shader to modify (e.g., `enemy_pack_hunter` or `behaviors/shaders/enemy_pack_hunter.nadir`)
- `--describe`: Just explain the hot-reload workflow without making changes

## How Hot-Reload Works

OdysseyEngine has built-in hot-reload for `.nadir` behavior shaders, enabled by default in `engine.xml`:
```xml
<nadir>
  <hot_reload>true</hot_reload>
</nadir>
```

The engine watches the `behaviors/shaders/` directory for file changes. When a `.nadir` file is saved:
1. The Nadir compiler automatically recompiles the modified shader to SPIR-V
2. The new SPIR-V module is loaded into the Vulkan compute pipeline
3. The next frame dispatch uses the updated shader
4. No engine restart is required
5. Entity state (positions, health, persistent memory) is preserved across reloads

The build system copies `behaviors/` to `T:/OdysseyEngine/build/behaviors/` via the `copy_assets` target. For hot-reload during development, you should edit the files in the **build directory** (`T:/OdysseyEngine/build/behaviors/shaders/`) since that is where the running engine watches. Alternatively, edit the source files and re-run the copy:
```bash
cmake --build T:/OdysseyEngine/build --config Release --target copy_assets
```

## Steps

### 1. Verify the engine is configured for hot-reload
Check `T:/OdysseyEngine/engine.xml` (or `T:/OdysseyEngine/build/engine.xml`):
```bash
grep hot_reload T:/OdysseyEngine/build/engine.xml
```
Should show `<hot_reload>true</hot_reload>`.

### 2. Identify the shader to modify
Resolve the shader path:
```bash
ls T:/OdysseyEngine/behaviors/shaders/<shader>.nadir
```

### 3. Read the current shader
Read the file to understand current behavior before making changes.

### 4. Make the modification
Edit the `.nadir` file in the **source directory** (`T:/OdysseyEngine/behaviors/shaders/<shader>.nadir`).

Common modifications:
- **Tune scoring weights**: Adjust multipliers in `combat_score`, `flee_score`, etc.
- **Change state thresholds**: Modify the `> 0.3` threshold in state transition logic
- **Adjust cooldowns**: Change `cooldown_start()` durations
- **Modify steering**: Adjust `steer_arrive()` distances, `steer_flock()` weights
- **Add new behavior**: Add a new state or scoring dimension

### 5. Copy to build directory
```bash
cp T:/OdysseyEngine/behaviors/shaders/<shader>.nadir T:/OdysseyEngine/build/behaviors/shaders/<shader>.nadir
```
Or rebuild the copy_assets target:
```bash
export VCToolsVersion=14.42.34433
cmake --build T:/OdysseyEngine/build --config Release --target copy_assets
```

### 6. Verify the reload
If the engine is running, it will automatically detect the change and recompile. Watch the engine console for:
- `[nadir] Detected change: <shader>.nadir`
- `[nadir] Recompiled <shader>.nadir (X ms)`
- Or an error message if compilation fails

### 7. Report
Tell the user:
- What was changed
- Whether the shader compiled successfully
- That the engine will pick up changes automatically if running
- If the engine is not running, suggest `/run` to launch it

## Tips
- Hot-reload preserves entity persistent state (`persist[]` buffer). New state fields default to 0.
- If a shader fails to compile during hot-reload, the engine continues using the previous version.
- Use `debug_state_color()` to visually verify behavior changes in real-time.
- The debug overlay (toggle in-engine) shows per-entity state and score values.
