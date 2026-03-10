# OdysseyEngine - Claude Code Project Guide

## Build
```bash
# Configure (vcpkg must be installed, VCPKG_ROOT set)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# Run
./build/Release/odyssey run --scene demo/scenes/shooter_arena.scene.xml
```

## Test
```bash
# Unit tests (no GPU required)
ctest --test-dir build -R unit

# Pipeline tests (requires GPU)
ctest --test-dir build -R pipeline
```

## Architecture
- **GPU-maximalist**: behavior AI runs as compute shaders (.nadir files)
- **Pure functions**: all C++ functions are pure; side effects isolated to I/O boundaries
- **Nadir system**: GLSL compute shaders evaluate all behaviors simultaneously with weighted scoring (not behavior trees)

## Key Directories
- `src/core/` - types, Result<T,E>
- `src/vulkan/` - Vulkan abstraction (instance, device, swapchain, buffer, pipeline, command)
- `src/nadir/` - behavior system (compiler, buffers, dispatch)
- `src/scene/` - scene/prefab loading, entity manager
- `src/scripting/` - C++ script system (Script, ScriptContext, ScriptResult)
- `src/net/` - networking (UDP socket, protocol, server, client, replication)
- `src/mcp/` - MCP server for Claude Code integration
- `src/debug/` - profiler, overlay, behavior replay
- `src/cli/` - CLI interface
- `src/app/` - engine main loop
- `behaviors/lib/` - GLSL include library
- `behaviors/shaders/` - .nadir behavior files
- `demo/` - shooter demo (scenes, prefabs, materials, scripts)

## Conventions
- C++20, `#pragma once`, namespaces: `odyssey::*`
- Include paths relative to `src/`
- Pure functions return values; impure wrappers commit to GPU/OS/network
- XML for all asset formats (scene, prefab, material, mesh)
- .nadir extension for behavior shaders (valid GLSL compute with auto-prepended preamble)
