# OdysseyEngine - Claude Code Project Guide

## Build
```bash
# CRITICAL: must set VCToolsVersion to match vcpkg binary ABI
export VCToolsVersion=14.42.34433

# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
VCToolsVersion=14.42.34433 cmake --build build --config Release

# Run
cd build && ./Release/odyssey.exe run
```

## Test
```bash
cd build && ./Release/odyssey_tests_unit.exe    # 118 unit tests
cd build && ./Release/odyssey_tests_pipeline.exe # pipeline tests (requires GPU)
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
