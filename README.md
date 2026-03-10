# OdysseyEngine

**A GPU-maximalist 3D engine where entity AI is authored in compute shaders.**

<!-- Badges: build status, license, Vulkan version -->

---

## Overview

OdysseyEngine is a Vulkan-based 3D engine built on a radical premise: **everything that can run on the GPU, does**. Its signature feature is **Nadir** -- a behavior system where entity AI is authored in GLSL compute shaders (`.nadir` files) instead of C++ behavior trees.

Traditional game engines run AI on the CPU, one entity at a time, walking a behavior tree to pick a single action per tick. OdysseyEngine flips this: thousands of entities evaluate all possible behaviors simultaneously on the GPU using pure math -- no branching, no tree traversal, no thread divergence.

### Design Principles

1. **Pure functions everywhere** -- All C++ functions are pure (deterministic, no side effects). Side effects are isolated to I/O boundaries.
2. **AI-agent first** -- Designed to be operated by an LLM (Claude Code). CLI-first interface, text-based asset formats (XML), MCP server integration.
3. **Full pipeline testing** -- Integration tests verify data flows from input buffers through compute dispatch to output buffers on real GPU hardware.

---

## Features

- **Nadir Behavior System** -- Author entity AI in GPU compute shaders. Score all behaviors simultaneously via math, output multiple weighted actions, achieve perfect GPU utilization.
- **Pure Function Architecture** -- Deterministic C++ core. Compute in pure functions, commit at I/O boundaries.
- **CLI-First Interface** -- Every operation available from the command line. JSON output mode for tooling integration.
- **Hot-Reload** -- Edit `.nadir` files and see behavior changes without restarting the engine.
- **XML Asset Formats** -- Human-readable, diff-friendly, LLM-friendly scene, prefab, and material definitions.
- **Vulkan Compute Pipeline** -- Direct Vulkan abstraction layer for SSBO management, compute dispatch, and synchronization.
- **Full Pipeline Tests** -- GPU round-trip tests that verify data flows correctly from CPU through compute shaders and back.

---

## Quick Start

### Prerequisites

| Dependency | Minimum Version | Notes |
|---|---|---|
| Vulkan SDK | 1.3+ | [lunarg.com/vulkan-sdk](https://vulkan.lunarg.com/) |
| CMake | 3.24+ | [cmake.org](https://cmake.org/) |
| vcpkg | latest | [github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg) |
| C++20 compiler | GCC 12+ / Clang 15+ / MSVC 17.4+ | Must support C++20 features |

### Clone and Build

```bash
# Clone the repository
git clone https://github.com/your-org/OdysseyEngine.git
cd OdysseyEngine

# Configure with CMake (vcpkg toolchain)
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run tests
ctest --test-dir build --output-on-failure
```

### Run the First Demo

```bash
# Run the 1000-agent flocking demo
./build/odyssey run --scene scenes/test_flock.scene.xml

# Run with validation layers enabled (debug)
./build/odyssey run --scene scenes/test_flock.scene.xml --validation

# Run headless (no window, for CI/testing)
./build/odyssey run --scene scenes/test_flock.scene.xml --headless --frames 100
```

---

## Architecture Overview

OdysseyEngine is structured in layers:

```
┌──────────────────────────────────────┐
│          CLI / Application           │
├──────────────────────────────────────┤
│         Nadir Behavior System        │
│   (compiler, buffers, dispatch)      │
├──────────────────────────────────────┤
│          Vulkan Abstraction          │
│  (instance, device, pipeline, cmd)   │
├──────────────────────────────────────┤
│          Core Types & Result         │
│     (pure value types, error handling│)
└──────────────────────────────────────┘
```

Each layer depends only on layers below it. All computation is pure; side effects (Vulkan calls, file I/O) happen only at the boundaries.

For a detailed architecture description, see [docs/architecture.md](docs/architecture.md).

---

## Nadir Behavior System

### What is Nadir?

Nadir is a behavior authoring system where entity AI runs as GPU compute shaders. Instead of writing C++ behavior trees, you write `.nadir` files -- GLSL compute shaders that read game state from SSBOs, score all possible behaviors using math, and write weighted action outputs.

### Why Not Behavior Trees?

| Aspect | Behavior Trees | Nadir |
|---|---|---|
| Execution | Walk tree, pick ONE leaf | Score ALL behaviors simultaneously |
| Branching | `if/else` at every node | Pure math, no branching |
| GPU fit | Thread divergence on branches | Perfect GPU utilization |
| Multi-action | One action per tick | Multiple weighted actions per tick |
| 1000 entities | 1000 tree walks (CPU) | 1 compute dispatch (GPU) |

**Example:** An enemy with 4 arms can independently score and fire each weapon simultaneously -- impossible with traditional behavior trees that pick a single action per tick.

### Writing Your First .nadir File

```glsl
// behaviors/shaders/simple_chase.nadir
//
// Nadir preamble is injected automatically:
//   - buffer bindings (EntityTransforms, EntityStats, WorldState, etc.)
//   - gl_GlobalInvocationID, entity index
//   - #include support for behavior libraries

#include "scoring.glsl"
#include "steering.glsl"

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;

    vec3 my_pos    = entity_transforms[idx].position.xyz;
    vec3 target    = world_state.player_position.xyz;
    float health   = entity_stats[idx].health;
    float distance = length(target - my_pos);

    // Score behaviors using utility curves (no branching!)
    float chase_score  = score_linear(distance, 50.0, 5.0);   // closer = higher
    float flee_score   = score_inverse(health, 0.0, 30.0);    // low health = flee
    float idle_score   = 0.1;                                  // baseline

    // Compute steering
    vec3 chase_vec = seek(my_pos, target);
    vec3 flee_vec  = flee(my_pos, target);

    // Output weighted actions (downstream blends by weight)
    behavior_output[idx].move_vector = normalize(
        chase_vec * chase_score + flee_vec * flee_score
    );
    behavior_output[idx].move_weight = max(chase_score, flee_score);
    behavior_output[idx].animation_id = (chase_score > flee_score) ? 1 : 2;
}
```

### Hot-Reload Workflow

1. Edit a `.nadir` file in your editor
2. The engine detects the file change via filesystem watch
3. `shaderc` recompiles GLSL to SPIR-V
4. The compute pipeline is recreated with the new shader
5. Behavior changes take effect on the next frame -- no restart required

For a complete authoring guide, see [docs/nadir-guide.md](docs/nadir-guide.md).

---

## CLI Reference

OdysseyEngine uses a CLI-first interface. Every operation is available from the command line.

### Core Commands

```bash
# Run the engine with a scene
odyssey run --scene <path> [--validation] [--headless] [--frames N]

# Compile a .nadir file to SPIR-V (no engine launch)
odyssey compile --shader <path.nadir> [--output <path.spv>]

# Validate a scene/prefab XML file against its schema
odyssey validate --file <path.xml>

# List all entities in a scene
odyssey inspect --scene <path.scene.xml>

# Run tests
odyssey test [--unit] [--pipeline] [--shader]

# Print engine info (Vulkan device, version, capabilities)
odyssey info
```

### Global Options

| Option | Description |
|---|---|
| `--config <path>` | Path to engine.xml config file (default: `./engine.xml`) |
| `--verbose` / `-v` | Enable verbose logging |
| `--json` | Output results as JSON (for tooling/MCP) |
| `--help` / `-h` | Show help for any command |

For the full CLI reference, see [docs/cli-reference.md](docs/cli-reference.md).

---

## Project Structure

```
OdysseyEngine/
├── CMakeLists.txt                 # Top-level CMake build
├── vcpkg.json                     # Dependency manifest (Vulkan, GLFW, etc.)
├── engine.xml                     # Engine runtime configuration
├── src/
│   ├── core/
│   │   ├── types.h                # Pure value types (vec3, EntityID, etc.)
│   │   └── result.h               # Result<T,E> monadic error handling
│   ├── vulkan/
│   │   ├── instance.h/cpp         # Vulkan instance + validation layers
│   │   ├── device.h/cpp           # Physical/logical device + VMA allocator
│   │   ├── swapchain.h/cpp        # Swapchain management
│   │   ├── buffer.h/cpp           # SSBO creation, staging, upload
│   │   ├── compute_pipeline.h/cpp # Compute pipeline from SPIR-V bytecode
│   │   └── command.h/cpp          # Command buffers, dispatch, barriers
│   ├── nadir/
│   │   ├── nadir_system.h/cpp     # Archetype registry, dispatch, hot-reload
│   │   ├── behavior_compiler.h/cpp# shaderc GLSL->SPIR-V, #include resolution
│   │   └── nadir_buffers.h/cpp    # SSBO layout computation + creation
│   ├── cli/
│   │   └── cli.h/cpp              # CLI argument parsing (CLI11)
│   └── app/
│       ├── engine.h/cpp           # Main loop, frame orchestration
│       └── main.cpp               # Entry point
├── behaviors/
│   ├── lib/
│   │   ├── scoring.glsl           # Utility curves, normalization helpers
│   │   └── steering.glsl          # Seek, flee, arrive, flocking behaviors
│   └── shaders/
│       └── test_flock.nadir       # Demo: 1000-agent flocking behavior
├── tests/
│   ├── unit/
│   │   ├── test_dispatch_config.cpp
│   │   ├── test_buffer_layout.cpp
│   │   └── test_behavior_compiler.cpp
│   └── pipeline/
│       └── test_nadir_pipeline.cpp # GPU round-trip integration test
├── schemas/
│   ├── scene.xsd                  # XML schema for scene files
│   └── prefab.xsd                 # XML schema for prefab files
└── docs/
    ├── architecture.md            # Detailed architecture document
    ├── nadir-guide.md             # Nadir behavior authoring guide
    ├── cli-reference.md           # Complete CLI reference
    └── diagrams/                  # Mermaid architecture diagrams
```

---

## Building & Testing

### Build Commands

```bash
# Debug build (validation layers, asserts)
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release build
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Running Tests

```bash
# All tests
ctest --test-dir build --output-on-failure

# Unit tests only (pure functions, no GPU required)
./build/odyssey_tests_unit

# Pipeline tests (requires Vulkan-capable GPU)
./build/odyssey_tests_pipeline

# Compile-check all .nadir files
odyssey test --shader
```

### Test Philosophy

- **Unit tests** verify pure functions: buffer layout computation, dispatch config calculation, shader compilation.
- **Pipeline tests** verify GPU round-trips: write known data to input SSBOs, dispatch a compute shader, read back output SSBOs, assert expected values.
- **Shader tests** verify that all `.nadir` files compile successfully to SPIR-V.

---

## Configuration

Engine runtime configuration lives in `engine.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<engine>
    <vulkan>
        <validation>true</validation>
        <device_index>0</device_index>
        <max_frames_in_flight>2</max_frames_in_flight>
    </vulkan>

    <nadir>
        <workgroup_size>256</workgroup_size>
        <behavior_path>behaviors/shaders</behavior_path>
        <library_path>behaviors/lib</library_path>
        <hot_reload>true</hot_reload>
    </nadir>

    <window>
        <width>1280</width>
        <height>720</height>
        <title>OdysseyEngine</title>
        <vsync>true</vsync>
    </window>

    <logging>
        <level>info</level>
        <pattern>[%Y-%m-%d %H:%M:%S.%e] [%l] %v</pattern>
    </logging>
</engine>
```

---

## Asset Formats

All assets use XML for human readability, diff-friendliness, and LLM compatibility.

### Scene File (`.scene.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<scene name="test_flock" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
       xsi:noNamespaceSchemaLocation="../schemas/scene.xsd">

    <world>
        <bounds min="-500 -100 -500" max="500 100 500" />
        <gravity>0 -9.81 0</gravity>
    </world>

    <entities>
        <archetype name="boid" count="1000" behavior="test_flock.nadir">
            <spawn>
                <region type="sphere" center="0 50 0" radius="100" />
                <velocity type="random" min_speed="2.0" max_speed="5.0" />
            </spawn>
            <stats health="100" speed="5.0" />
        </archetype>

        <entity name="player" prefab="player.prefab.xml">
            <transform position="0 1 0" rotation="0 0 0" scale="1 1 1" />
        </entity>
    </entities>
</scene>
```

### Prefab File (`.prefab.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<prefab name="player" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
        xsi:noNamespaceSchemaLocation="../schemas/prefab.xsd">

    <mesh src="meshes/player.obj" />
    <material src="materials/player.mat.xml" />

    <components>
        <health max="100" regen_rate="1.0" />
        <inventory capacity="20" />
    </components>
</prefab>
```

### Material File (`.mat.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<material name="player_material">
    <shader vertex="shaders/pbr.vert.spv" fragment="shaders/pbr.frag.spv" />
    <textures>
        <texture slot="albedo" src="textures/player_albedo.png" />
        <texture slot="normal" src="textures/player_normal.png" />
        <texture slot="roughness" src="textures/player_rough.png" />
    </textures>
    <properties>
        <metallic>0.0</metallic>
        <roughness>0.5</roughness>
    </properties>
</material>
```

---

## Roadmap

### Phase 1: Foundation + Minimum Viable Nadir (Current)

- Vulkan instance, device, swapchain setup
- SSBO creation and management
- Compute pipeline from SPIR-V
- Nadir system: compile `.nadir` files, bind buffers, dispatch
- Pipeline tests: CPU write -> GPU compute -> CPU readback
- CLI with `run`, `compile`, `test` commands

### Phase 2: Scene System + Asset Pipeline + C++ Scripting

- XML scene and prefab loading (pugixml)
- Schema validation (XSD)
- C++ scripting system (quests, dialogue, UI, inventory, save/load)
- Mesh and material loading

### Phase 3: Rich Behavior Authoring + Nadir Library

- Expanded scoring.glsl library (sigmoid, bell curve, step, cooldown)
- Expanded steering.glsl library (arrive, wander, obstacle avoidance, formation)
- State machine support in shaders (persistent state buffer)
- Example behaviors: pack hunting, ranged combat, multi-arm gunner

### Phase 4: MCP Server + Skills

- Model Context Protocol server for Claude Code integration
- Skill definitions for common engine operations
- Automated behavior authoring from natural language

### Phase 5: Networking & Multiplayer

- Authoritative server architecture
- Nadir running on both client (prediction) and server (authority)
- Entity interpolation and reconciliation

### Phase 6: Debug, Profile, Polish

- Debug visualization overlay (behavior scores, steering vectors)
- GPU profiling (pipeline statistics, timestamp queries)
- Performance optimization pass

### Phase 7: Capstone -- Multi-Armed Shooter Demo (Multiplayer)

- Enemies with multiple independent arms, each scored via Nadir
- Multiplayer arena with authoritative server
- Full showcase of the Nadir system's capabilities

---

## Contributing

### Code Conventions

- **C++20** with modules where supported
- **Pure functions** -- all computation must be deterministic with no side effects
- **Result\<T,E\>** for error handling -- no exceptions
- **snake_case** for functions and variables, **PascalCase** for types
- **No raw pointers** -- use RAII wrappers, `std::unique_ptr`, or value types
- **Format with clang-format** before committing

### Commit Messages

Use conventional commit format:

```
feat(nadir): add obstacle avoidance to steering library
fix(vulkan): handle device lost during swapchain recreation
test(pipeline): add GPU round-trip test for flocking behavior
docs: update Nadir authoring guide with state machine examples
```

### Pull Request Process

1. Create a feature branch from `main`
2. Ensure all tests pass (`ctest --test-dir build --output-on-failure`)
3. Ensure all `.nadir` files compile (`odyssey test --shader`)
4. Submit PR with description of changes

---

## License

*License TBD -- see LICENSE file when available.*
