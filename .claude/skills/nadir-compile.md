# /nadir-compile

Compile a .nadir behavior shader to SPIR-V using the engine CLI.

## Usage
`/nadir-compile <name_or_path>`

## Arguments
- `<name_or_path>`: Either:
  - A shader name without extension (e.g., `enemy_pack_hunter`) -- resolves to `behaviors/shaders/enemy_pack_hunter.nadir`
  - A full relative or absolute path to a `.nadir` file

## Prerequisites
- The engine executable must be built: `T:/OdysseyEngine/build/Release/odyssey.exe`
- If it is not built, inform the user to run `/build` first.

## Steps

### 1. Resolve the shader path
If the argument is just a name, expand it:
```
<name> -> behaviors/shaders/<name>.nadir
```
Verify the file exists:
```bash
ls T:/OdysseyEngine/behaviors/shaders/<name>.nadir
```

### 2. Compile using the engine CLI
The working directory must be `T:/OdysseyEngine/build` so the engine can find `behaviors/lib/` for includes:
```bash
cd T:/OdysseyEngine/build && ./Release/odyssey.exe nadir compile behaviors/shaders/<name>.nadir
```

### 3. Validate (optional, for syntax-only check without emitting SPIR-V)
```bash
cd T:/OdysseyEngine/build && ./Release/odyssey.exe nadir validate behaviors/shaders/<name>.nadir
```

## Interpreting Output

### Success
The compiler will report success and the path to the emitted SPIR-V binary. Report this to the user.

### Compilation errors
Common errors and fixes:

**Undeclared identifier**: A variable or function is used without being defined or included. Check that all needed `#include` directives are present. Available includes: `scoring.glsl`, `steering.glsl`, `spatial.glsl`, `state_machine.glsl`, `blackboard.glsl`, `debug.glsl`.

**Type mismatch**: GLSL is strict about types. Common issues:
- `float` vs `int` -- use `float(x)` or `int(x)` casts
- `vec3` vs `vec4` -- use `.xyz` to downcast or `vec4(v, w)` to upcast
- `uint` vs `int` -- use `uint(x)` or `int(x)`

**Missing main()**: Every `.nadir` file must have a `void main()` function.

**Buffer access out of bounds**: Ensure `idx < total_entities` guard is present at the top of `main()`.

**Unknown include**: The include file must exist in `T:/OdysseyEngine/behaviors/lib/`. Check spelling and extension.

## Notes
- The Nadir compiler auto-prepends a preamble that defines buffer layouts, uniforms, and the compute shader header. You do not need to write `#version`, `layout()`, or buffer declarations in `.nadir` files.
- Hot-reload is enabled by default in the engine. If the engine is running, modifying and saving a `.nadir` file will trigger automatic recompilation and reload without restarting.
