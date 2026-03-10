# OdysseyEngine Architecture

This document provides a detailed technical overview of OdysseyEngine's architecture, intended for developers contributing to the engine and for LLM agents (Claude Code) operating it.

---

## Table of Contents

1. [Design Philosophy](#design-philosophy)
2. [System Architecture](#system-architecture)
3. [Nadir Deep Dive](#nadir-deep-dive)
4. [Vulkan Abstraction Layer](#vulkan-abstraction-layer)
5. [Pure Function Architecture](#pure-function-architecture)
6. [C++ Scripting System](#c-scripting-system-phase-2)
7. [Testing Strategy](#testing-strategy)
8. [Multiplayer Architecture](#multiplayer-architecture-phase-5)

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
┌─────────────────────────────────────────────────────────┐
│                    CLI / Application                     │
│              (cli.h, engine.h, main.cpp)                │
├──────────────────────┬──────────────────────────────────┤
│   Nadir System       │        Scene / Assets            │
│ (nadir_system.h)     │     (Phase 2: loaders)           │
│ (behavior_compiler.h)│                                  │
│ (nadir_buffers.h)    │                                  │
├──────────────────────┴──────────────────────────────────┤
│                  Vulkan Abstraction                      │
│    instance.h  device.h  swapchain.h  buffer.h          │
│    compute_pipeline.h  command.h                        │
├─────────────────────────────────────────────────────────┤
│                    Core Types                            │
│              types.h  result.h                          │
└─────────────────────────────────────────────────────────┘
```

### Dependency Flow

Dependencies flow strictly downward:

- `app/` depends on `nadir/`, `vulkan/`, `cli/`, `core/`
- `nadir/` depends on `vulkan/`, `core/`
- `vulkan/` depends on `core/`
- `core/` depends on nothing (pure types only)

No circular dependencies are permitted. Each layer exposes a minimal API to the layer above.

### Frame Loop

A single frame in OdysseyEngine follows this sequence:

1. **Input** -- Poll GLFW for window events
2. **CPU Update** -- Run C++ scripts (Phase 2), update world state
3. **Nadir Dispatch** -- For each archetype:
   - Update shared SSBOs (world state, transforms, stats)
   - Bind per-archetype SSBOs (persistent state, output, debug)
   - Record compute dispatch command
   - Submit command buffer with appropriate barriers
4. **Readback** -- Read behavior output SSBOs
5. **Apply** -- Apply movement, combat, animation from behavior outputs
6. **Render** -- Submit render commands (swapchain acquire, draw, present)

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
- Handles window resize (swapchain recreation)
- Pure function: `choose_swap_extent(capabilities, window_size) -> VkExtent2D`
- I/O boundary: `create_swapchain(device, surface, config) -> VkSwapchainKHR`

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
# Compiles every .nadir file in behaviors/shaders/
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
