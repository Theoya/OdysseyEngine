# OdysseyEngine Architecture

This document provides a detailed technical overview of OdysseyEngine's architecture, intended for developers contributing to the engine and for LLM agents (Claude Code) operating it.

---

## Table of Contents

1. [Design Philosophy](#design-philosophy)
2. [System Architecture](#system-architecture)
3. [Engine/Game Separation](#enginegame-separation)
4. [Rendering Pipeline](#rendering-pipeline)
5. [Nadir Deep Dive](#nadir-deep-dive)
6. [Vulkan Abstraction Layer](#vulkan-abstraction-layer)
7. [Pure Function Architecture](#pure-function-architecture)
8. [C++ Scripting System](#c-scripting-system-phase-2)
9. [Testing Strategy](#testing-strategy)
10. [Multiplayer Architecture](#multiplayer-architecture-phase-5)

---

## Design Philosophy

### GPU-Maximalist

OdysseyEngine follows a single architectural axiom: **if it can run on the GPU, it must run on the GPU**. This applies to:

- Entity behavior evaluation (Nadir compute shaders)
- Spatial queries (spatial grid in SSBOs)
- Score normalization and blending (shader math)
- Physics-adjacent AI (steering, flocking, avoidance)

The CPU's role is limited to orchestration: binding buffers, issuing dispatches, handling I/O, and running inherently sequential logic (quest state machines, UI, save/load).

### Pure Functions

Every C++ function in the engine is pure: given the same inputs, it produces the same outputs, with no side effects. This design choice provides:

- **Testability** -- pure functions can be tested in isolation without mocking
- **Determinism** -- the engine produces identical results given identical inputs
- **Parallelism** -- pure functions are safe to call from any thread
- **LLM compatibility** -- pure functions are easier for AI agents to reason about

Side effects (Vulkan API calls, file I/O, window management) are isolated to thin I/O boundary wrappers that call pure functions to compute what to do, then execute the side effects.

### AI-Agent First

The engine is designed to be operated by an LLM (Claude Code) as a first-class use case:

- **CLI-first** -- every operation has a command-line interface, no GUI required
- **Text-based assets** -- XML formats are readable and editable by LLMs
- **JSON output mode** -- CLI commands can output JSON for programmatic consumption
- **MCP server** (Phase 4) -- Model Context Protocol integration for direct Claude Code operation
- **Deterministic behavior** -- pure functions make the engine predictable for AI agents

---

## System Architecture

### Layer Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                     Game (demo/, template/)                    │
│          ShooterGame implements Game interface                 │
│            create_game() factory function                      │
├──────────────────────────────────────────────────────────────┤
│                    CLI / Application                           │
│           (cli.h, engine.h, odyssey_main.cpp)                │
├──────────────────┬──────────────────┬────────────────────────┤
│   Nadir System   │  Rendering       │   Scene / Assets       │
│ (nadir_system.h) │  Renderer        │   (loaders, XML)       │
│ (compiler)       │  PostProcessor   │                        │
│ (buffers)        │  Camera, Input   │                        │
├──────────────────┴──────────────────┴────────────────────────┤
│                  Vulkan Abstraction                            │
│   instance  device  swapchain  buffer  command                │
│   compute_pipeline  renderer  postprocess                    │
├──────────────────────────────────────────────────────────────┤
│                    Core Types                                 │
│              types.h  result.h                               │
└──────────────────────────────────────────────────────────────┘
```

### Dependency Flow

Dependencies flow strictly downward:

- `demo/` depends on `app/` (Game interface only)
- `app/` depends on `nadir/`, `vulkan/`, `cli/`, `scene/`, `core/`
- `nadir/` depends on `vulkan/`, `core/`
- `vulkan/` depends on `core/`
- `core/` depends on nothing (pure types only)

No circular dependencies are permitted. Each layer exposes a minimal API to the layer above. The engine has **zero references** to game code (`demo/`) — the game registers itself via the `create_game()` factory function.

### Frame Loop

A single frame in OdysseyEngine follows this sequence:

1. **Input** -- Poll GLFW for window events, check F11 (fullscreen toggle)
2. **Fence Wait** -- Wait for previous frame's GPU work to complete
3. **Game Tick** -- Call `Game::on_tick()` — readback, update logic, physics, AI
4. **Acquire** -- `vkAcquireNextImageKHR` — if `OUT_OF_DATE`, recreate swapchain and skip frame
5. **Nadir Dispatch** -- For each archetype, record compute dispatch
6. **Render** -- Render scene to PostProcessor offscreen target, then CRT+EVA post-process to swapchain
7. **Present** -- `vkQueuePresentKHR` — if `OUT_OF_DATE`/`SUBOPTIMAL`/resize flag, recreate swapchain

---

## Engine/Game Separation

OdysseyEngine follows a Unity-like model: the engine is a static library (`odyssey_engine`), and games are standalone executables that link against it.

### Game Interface

Games implement the `Game` abstract class:

```cpp
class Game {
    virtual Result<bool> on_init(GameContext& ctx) = 0;
    virtual void on_tick(GameContext& ctx) = 0;
    virtual const std::vector<RenderEntity>& get_renderables() const = 0;
    virtual HUDParams get_hud_params() const = 0;
    virtual void on_shutdown() = 0;
};
```

And provide a factory function:

```cpp
std::unique_ptr<Game> odyssey::create_game();
```

The engine-provided entry point (`odyssey_main.cpp`) calls `create_game()`, boots the engine, and runs the main loop. Games never touch Vulkan directly.

### Project Structure

```
OdysseyEngine/
├── src/                    # Engine source (odyssey_engine library)
├── behaviors/lib/          # Engine GLSL library (scoring, steering, etc.)
├── shaders/                # Engine post-process shaders (CRT, EVA HUD)
├── demo/                   # Shooter game (links against engine)
│   ├── behaviors/          # Game-specific .nadir behavior shaders
│   ├── scenes/             # Game scenes (.scene.xml)
│   └── shooter_game.cpp    # Game implementation
├── template/               # Template for new games
│   ├── CMakeLists.txt      # Standalone build (add_subdirectory of engine)
│   ├── my_game.h/cpp       # Minimal Game implementation
│   ├── engine.xml          # Game-specific config
│   └── scenes/             # Game scenes
└── engine.xml              # Runtime config (behavior_dir, scene path, etc.)
```

### Configuration

`engine.xml` provides runtime configuration:

```xml
<engine version="1">
  <window width="1920" height="1080" title="MyGame" vsync="true" fullscreen="false"/>
  <nadir behavior_dir="demo/behaviors" lib_dir="behaviors/lib" hot_reload="true"/>
  <scene path="demo/scenes/shooter_arena.scene.xml"/>
</engine>
```

The `scene_path` and `behavior_dir` are game-specific — the engine has no hardcoded paths to game content.

---

## Rendering Pipeline

### Forward Renderer with Post-Processing

The rendering pipeline has two paths:

1. **With PostProcessor** (default): Scene renders to an offscreen target, then CRT + EVA HUD effects are applied as full-screen triangle passes before presenting to the swapchain.
2. **Direct** (fallback): Scene renders directly to swapchain framebuffers.

### Window Resize and Fullscreen

The engine handles window resize and fullscreen (F11 toggle) via swapchain recreation:

1. **Detection**: `glfwSetFramebufferSizeCallback` sets `framebuffer_resized_` flag; `vkAcquireNextImageKHR` / `vkQueuePresentKHR` return `VK_ERROR_OUT_OF_DATE_KHR`
2. **Recreation flow**:
   - `vkDeviceWaitIdle()` — flush all GPU work
   - Handle minimization (0x0 framebuffer) by waiting for events
   - Recreate swapchain (pass old handle for driver resource recycling)
   - `Renderer::recreate_for_resize()` — new depth buffer + framebuffers
   - `PostProcessor::recreate_for_resize()` — new offscreen target + descriptor update + framebuffers
3. **Fence safety**: `vkResetFences` is deferred to after successful acquire to prevent deadlock when acquire returns `OUT_OF_DATE`

### Fullscreen Toggle

F11 toggles between windowed and exclusive fullscreen via `glfwSetWindowMonitor()`. The windowed position/size are saved on enter and restored on exit. The toggle sets `framebuffer_resized_` to trigger swapchain recreation.

---

## Nadir Deep Dive

### Overview

Nadir replaces traditional CPU-side behavior trees with GPU compute shaders. Each entity archetype (e.g., "boid", "soldier", "turret") has a `.nadir` file that defines its behavior. At runtime, the Nadir system compiles these files to SPIR-V, creates compute pipelines, binds SSBOs containing game state, and dispatches compute work.

### Data Flow Per Frame

```
CPU (C++)                          GPU (Compute)
─────────                          ─────────────
1. Write WorldState to SSBO ──────→ Buffer 3 (read-only)
2. Write EntityTransforms   ──────→ Buffer 0 (read-only)
3. Write EntityStats        ──────→ Buffer 1 (read-only)
4. Write SpatialGrid        ──────→ Buffer 2 (read-only)
                                    │
5. vkCmdDispatch ──────────────────→ .nadir shader executes
                                    │  - reads buffers 0-4
                                    │  - scores all behaviors
                                    │  - computes steering
                                    │  - writes outputs
                                    │
6. Read BehaviorOutput ←───────────── Buffer 5 (write-only)
7. Read DebugOutput    ←───────────── Buffer 6 (write-only)
                                    │
8. Apply to game state              Buffer 4 (read/write)
                                    persists between frames
```

### Buffer Layout

Nadir uses 7 SSBOs (Shader Storage Buffer Objects) with SoA (Structure of Arrays) layout for cache coherence:

| Binding | Name | Access | Size | Contents |
|---|---|---|---|---|
| 0 | `EntityTransforms` | Read | 16 bytes/entity | `vec4 position` (xyz + padding) |
| 1 | `EntityStats` | Read | 32 bytes/entity | `float health, ammo, stamina, speed` + reserved |
| 2 | `SpatialGrid` | Read | 16 bytes/cell | Cell occupancy, neighbor lists |
| 3 | `WorldState` | Read | Constant | `float time, delta_time; vec4 player_position; uint entity_count, frame;` |
| 4 | `AgentPersistState` | Read/Write | 64 bytes/entity | `uint state_id; float[4] timers; float[8] memory;` |
| 5 | `BehaviorOutput` | Write | 64 bytes/entity | `vec3 move_vector; float move_weight; uint attack_target; float attack_weight; uint animation_id; vec4 comms_data;` |
| 6 | `DebugOutput` | Write | 16 bytes/entity | `vec4 debug_color;` visualization data |

**Why SoA?** GPU compute shaders access memory in coalesced patterns. When 256 threads in a workgroup all read `position`, SoA layout means those reads are contiguous in memory, maximizing cache line utilization. AoS layout would interleave position with other fields, wasting bandwidth.

### Weighted Multi-Action Model

Traditional behavior trees pick ONE action per tick:

```
Root Selector
├── Sequence: Attack  →  if (has_ammo && enemy_visible) → ATTACK ✓
├── Sequence: Flee    →  if (low_health) → FLEE ✗ (not reached)
└── Sequence: Patrol  →  PATROL ✗ (not reached)
```

Nadir scores ALL behaviors simultaneously and outputs weighted actions:

```glsl
// All scores computed in parallel, no branching
float attack_score = score_linear(distance_to_enemy, 50.0, 5.0) * has_ammo;
float flee_score   = score_inverse(health, 0.0, 30.0);
float patrol_score = 0.1;
float comms_score  = score_step(nearby_allies, 2.0) * (1.0 - health/100.0);

// Output ALL actions with weights -- downstream systems blend
output.move_vector   = normalize(attack_dir * attack_score + flee_dir * flee_score);
output.move_weight   = max(attack_score, flee_score);
output.attack_target = nearest_enemy_id;
output.attack_weight = attack_score;
output.comms_data    = vec4(my_pos, health);  // broadcast to allies
```

The downstream system receives all weighted actions and can:
- Apply the highest-weighted movement
- Fire weapons if `attack_weight > threshold`
- Blend animations based on relative weights
- Broadcast communications if `comms_weight > 0`

This means an entity with 4 arms can independently score and fire each weapon simultaneously -- each arm's behavior is a separate output channel, all evaluated in the same shader invocation.

### Hybrid Scoring + State Machines

Nadir supports persistent state through the `AgentPersistState` buffer (binding 4). This enables finite state machines within shaders:

```glsl
uint current_state = agent_state[idx].state_id;
float timer = agent_state[idx].timers[0];

// State-dependent scoring modifiers
float aggression = (current_state == STATE_BERSERK) ? 2.0 : 1.0;

// State transitions via scoring (not branching)
float berserk_score = score_step(kills_this_wave, 5.0) * score_inverse(health, 0.0, 50.0);
float calm_score    = score_linear(health, 50.0, 100.0) * score_linear(timer, 10.0, 30.0);

// Highest score wins the transition
agent_state[idx].state_id = (berserk_score > calm_score) ? STATE_BERSERK : STATE_CALM;
agent_state[idx].timers[0] = timer + world_state.delta_time;
```

### Hot-Reload Mechanism

1. The `NadirSystem` monitors `.nadir` file timestamps each frame (or via OS file watcher)
2. When a change is detected:
   a. `BehaviorCompiler::compile()` is called with the modified file path
   b. `shaderc` compiles GLSL to SPIR-V, resolving `#include` directives against `behaviors/lib/`
   c. If compilation succeeds, the old `VkPipeline` is destroyed
   d. A new `VkComputePipeline` is created from the fresh SPIR-V
   e. The pipeline is swapped atomically for the next frame
3. If compilation fails, the error is logged and the old pipeline continues running

---

## Vulkan Abstraction Layer

The Vulkan layer provides thin, RAII-based wrappers around Vulkan objects. Each wrapper is responsible for a single concern.

### Instance (`vulkan/instance.h`)

- Creates `VkInstance` with required extensions
- Enables validation layers in debug builds
- Sets up debug messenger for validation output
- Pure function: `compute_required_extensions(config) -> vector<const char*>`
- I/O boundary: `create_instance(extensions) -> VkInstance`

### Device (`vulkan/device.h`)

- Selects physical device based on capabilities (compute queue, memory)
- Creates logical device with required queues
- Initializes VMA (Vulkan Memory Allocator) for buffer allocation
- Pure function: `score_physical_device(device, requirements) -> int`
- I/O boundary: `create_device(physical_device) -> VkDevice`

### Swapchain (`vulkan/swapchain.h`)

- Creates and manages the swapchain for presentation
- Handles window resize via `create_swapchain(old_swapchain)` for seamless recreation
- `destroy_swapchain()` cleans up image views and swapchain handle
- Pure function: `compute_swapchain_config(device, surface, width, height, vsync) -> SwapchainConfig`
- I/O boundary: `create_swapchain(device_ctx, surface, config, old_swapchain) -> SwapchainContext`

### Buffer (`vulkan/buffer.h`)

- Creates SSBOs for Nadir with appropriate memory flags
- Handles staging buffer uploads (host-visible staging -> device-local SSBO)
- Pure function: `compute_buffer_size(entity_count, stride) -> VkDeviceSize`
- I/O boundary: `create_buffer(allocator, size, usage) -> Buffer`

### Compute Pipeline (`vulkan/compute_pipeline.h`)

- Creates compute pipelines from SPIR-V bytecode
- Manages descriptor set layouts for SSBO bindings
- Pure function: `create_descriptor_layout_info(binding_count) -> VkDescriptorSetLayoutCreateInfo`
- I/O boundary: `create_compute_pipeline(device, spirv, layout) -> VkPipeline`

### Command (`vulkan/command.h`)

- Records and submits command buffers
- Manages memory barriers between compute dispatches
- Pure function: `compute_dispatch_groups(entity_count, workgroup_size) -> uint32_t`
- I/O boundary: `submit_command_buffer(queue, cmd_buf, fence) -> void`

---

## Pure Function Architecture

### The Pattern

Every operation in OdysseyEngine follows this pattern:

```cpp
// 1. Pure function: compute what to do
auto config = compute_dispatch_config(entity_count, workgroup_size);
// config is a plain struct, no side effects occurred

// 2. I/O boundary: do it
dispatch_compute(cmd_buffer, pipeline, config);
// This is the ONLY place a Vulkan call happens
```

### Why This Matters

**For testing:**
```cpp
// Test the pure function without any Vulkan setup
TEST(DispatchConfig, ComputesCorrectGroupCount) {
    auto config = compute_dispatch_config(1000, 256);
    EXPECT_EQ(config.group_count_x, 4);  // ceil(1000/256) = 4
    EXPECT_EQ(config.group_count_y, 1);
    EXPECT_EQ(config.group_count_z, 1);
}
```

**For determinism:**
```cpp
// Same inputs always produce same outputs
auto a = compute_buffer_layout(entity_count, archetype_config);
auto b = compute_buffer_layout(entity_count, archetype_config);
assert(a == b);  // Always true
```

**For LLM operation:**
```cpp
// Claude Code can predict function outputs without running the engine
// "compute_dispatch_config(1000, 256) returns {4, 1, 1}"
// This is always correct -- no hidden state to worry about
```

### Enforcement

Purity is enforced by convention and code review:
- Pure functions take inputs by value or const reference
- Pure functions return results by value
- Pure functions never call Vulkan/GLFW/file I/O functions
- Pure functions are marked `[[nodiscard]]` (the result IS the point)
- I/O boundary functions are clearly named and documented as such

---

## C++ Scripting System (Phase 2)

The C++ scripting system handles game logic that is inherently sequential and cannot benefit from GPU parallelism: quest state machines, dialogue trees, UI logic, inventory management, save/load.

### ScriptContext / ScriptResult Model

```cpp
// ScriptContext: immutable snapshot of game state (pure input)
struct ScriptContext {
    float delta_time;
    const EntityDatabase& entities;
    const QuestState& quests;
    const PlayerState& player;
    const InputState& input;
};

// ScriptResult: describes what should change (pure output)
struct ScriptResult {
    std::vector<EntityMutation> entity_mutations;
    std::vector<QuestUpdate> quest_updates;
    std::vector<UICommand> ui_commands;
    std::optional<std::string> scene_transition;
};

// Script: pure function from context to result
class QuestScript {
public:
    [[nodiscard]]
    ScriptResult evaluate(const ScriptContext& ctx) const;
};
```

The engine calls `evaluate()` (pure), then applies the `ScriptResult` at the I/O boundary. Scripts never directly modify game state -- they describe desired changes.

---

## Testing Strategy

### Unit Tests (No GPU Required)

Unit tests verify pure functions in isolation. They require no Vulkan device, no window, no file system. They are fast and run in CI on any machine.

**What they test:**
- `compute_dispatch_config()` returns correct workgroup counts
- `compute_buffer_layout()` returns correct sizes and offsets
- `BehaviorCompiler` resolves `#include` directives correctly
- `Result<T,E>` monadic operations chain correctly
- XML parsing produces correct data structures

**Example:**
```cpp
TEST(BufferLayout, ComputesCorrectSizeForArchetype) {
    ArchetypeConfig config{.entity_count = 1000};
    auto layout = compute_buffer_layout(config);

    EXPECT_EQ(layout.transforms_size, 1000 * 16);   // vec4 per entity
    EXPECT_EQ(layout.stats_size, 1000 * 32);         // 8 floats per entity
    EXPECT_EQ(layout.output_size, 1000 * 64);        // BehaviorOutput per entity
    EXPECT_EQ(layout.total_size(),
              layout.transforms_size + layout.stats_size +
              layout.spatial_grid_size + layout.world_state_size +
              layout.persist_state_size + layout.output_size +
              layout.debug_size);
}
```

### Pipeline Tests (GPU Required)

Pipeline tests verify the complete data flow: CPU writes data to input SSBOs, dispatches a compute shader, reads back output SSBOs, and asserts expected values. These require a Vulkan-capable GPU.

**What they test:**
- Data written by CPU arrives correctly in the shader
- Shader outputs are written to the correct SSBO locations
- Memory barriers correctly synchronize compute and readback
- Hot-reload produces correct pipeline replacement

**Example:**
```cpp
TEST(NadirPipeline, FlockingProducesNonZeroMovement) {
    // Setup: create Vulkan device, compile test_flock.nadir
    auto device = create_test_device();
    auto pipeline = compile_and_create_pipeline(device, "test_flock.nadir");

    // Write known input: 1000 entities in a sphere
    auto transforms = generate_sphere_positions(1000, /*radius=*/100.0f);
    upload_to_ssbo(device, buffers.transforms, transforms);

    // Dispatch
    dispatch_compute(device, pipeline, buffers, /*entity_count=*/1000);

    // Readback and verify
    auto outputs = readback_ssbo<BehaviorOutput>(device, buffers.output);
    for (const auto& out : outputs) {
        // Every boid should have non-zero movement from flocking
        EXPECT_GT(glm::length(out.move_vector), 0.0f);
        EXPECT_GT(out.move_weight, 0.0f);
    }
}
```

### Shader Tests

Shader tests verify that all `.nadir` files in the repository compile successfully to SPIR-V. This catches syntax errors and missing `#include` dependencies.

```bash
odyssey test --shader
# Compiles every .nadir file in demo/behaviors/
# Reports compilation errors with line numbers
# Exit code 0 = all pass, 1 = any failure
```

---

## Multiplayer Architecture (Phase 5)

### Authoritative Server

OdysseyEngine uses an authoritative server model for multiplayer:

- **Server** runs the full engine including Nadir dispatch (source of truth)
- **Client** runs Nadir locally for prediction (smooth visuals)
- **Reconciliation** corrects client state when server authority diverges

### Nadir on Client and Server

```
Client                              Server
──────                              ──────
1. Predict: dispatch Nadir locally   1. Dispatch Nadir (authoritative)
2. Apply predicted movement          2. Apply authoritative movement
3. Render with predicted state       3. Send authoritative state
4. Receive server state ◄──────────── 4. Broadcast to clients
5. Reconcile: blend toward server
```

Because Nadir behaviors are pure functions of their SSBO inputs, running the same `.nadir` shader with the same inputs on client and server produces identical outputs. This makes prediction highly accurate -- reconciliation corrections are typically small.

### Why Nadir Helps Multiplayer

- **Deterministic** -- same inputs produce same outputs on client and server
- **Bandwidth-efficient** -- send SSBO deltas (entity positions, stats) rather than behavior decisions
- **Server-scalable** -- server GPU handles thousands of AI entities via compute dispatch, not CPU iteration

---

## Scene XML round-trip

**Contract:** `scene::load_scene_file(path) -> scene::serialize_scene(scene, path)` produces byte-identical output for any unmutated SceneData. Lighting stubs, audio stubs, custom material overrides, scene-root attrs, comments, and indentation all survive the round trip.

**Implementation** (see `src/scene/scene_loader.{h,cpp}` and `src/scene/scene_serializer.{h,cpp}`):

- Loader reads the file in BINARY mode (CRLF preserved on Windows) and stores the raw text in `SceneData::preserved_source`.
- Loader extracts known fields into `SceneData::EntityDesc`, and routes anything it doesn't recognize into two per-entity buckets:
  - `unknown_attributes` -- `vector<pair<name, value>>`, insertion order preserved
  - `unknown_children_xml` -- raw serialized XML strings (no pugi handles outliving parse)
- Scene-root unknown attributes land in `SceneData::unknown_scene_attributes`.
- Serializer has two paths:
  - **Echo** (default for unmutated loads): writes `preserved_source` verbatim -- byte-identical guaranteed.
  - **Reconstruction** (Phase 4+ authoring, or `SerializeOptions::force_reconstruct`): emits XML in the documented stable order: known attrs -> unknown attrs (insertion order) -> known children -> unknown children.

**Ordering rule for the reconstruction path:**

```
<element known_attr_1 known_attr_2 ... unknown_attr_1 unknown_attr_2 ...>
    <known_child_1/>
    <known_child_2/>
    ...
    <unknown_child_1/>
    ...
</element>
```

Round-trip is regression-tested against both `demo/showcase/showcase.scene.xml` (deep preserve-unknowns coverage) and `demo/scenes/shooter_arena.scene.xml` (shooter-shape regression) in `tests/unit/scene/test_scene_serializer.cpp`. The `mutated` flag flips SceneData into reconstruction-on-serialize -- this is the hook the Phase 4 Inspector edits will use.
