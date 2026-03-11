# /run

Run the OdysseyEngine with a specific scene.

## Usage
`/run [<scene>] [--windowed] [--no-vsync]`

## Arguments
- `<scene>`: Path to a `.scene.xml` file, relative to the project root (default: `demo/scenes/shooter_arena.scene.xml`)
- `--windowed`: Hint to mention windowed mode (engine reads from `engine.xml`)
- `--no-vsync`: Hint to disable vsync (engine reads from `engine.xml`)

## Prerequisites
- The engine must be built first. Check that `T:/OdysseyEngine/build/Release/odyssey.exe` exists.
- If it does not exist, inform the user they need to build first (use `/build`).

## Steps

### 1. Verify the executable exists
```bash
ls T:/OdysseyEngine/build/Release/odyssey.exe
```
If missing, tell the user to run `/build` first.

### 2. Verify the scene file exists
```bash
ls T:/OdysseyEngine/<scene>
```
If the scene path is just a name (e.g., `shooter_arena`), expand it to `demo/scenes/shooter_arena.scene.xml`.

### 3. Run the engine
The working directory MUST be `T:/OdysseyEngine/build` so the engine can find its runtime assets (behaviors/, demo/, shaders/, schemas/, engine.xml) which are copied there by the build system.

```bash
cd T:/OdysseyEngine/build && ./Release/odyssey.exe run --scene demo/scenes/<scene>.scene.xml
```

The engine loads:
- `engine.xml` for window, Vulkan, and nadir configuration
- The scene file for entity layout
- Behavior shaders from `behaviors/shaders/`
- Rendering shaders from `shaders/`
- Prefabs from `demo/prefabs/`

### 4. Report
Tell the user the engine is running with the specified scene. If the command exits with a non-zero code, report the error output.

## Notes
- The engine requires a GPU with Vulkan support.
- Hot-reload is enabled by default in `engine.xml`. Modifying `.nadir` files while the engine runs will automatically recompile and reload behavior shaders.
- The MCP server is disabled by default. It can be enabled in `engine.xml` for Claude Code integration.
