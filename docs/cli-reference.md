# CLI Reference

OdysseyEngine uses a CLI-first interface built on [CLI11](https://github.com/CLIUtils/CLI11). Every engine operation is available from the command line, with JSON output mode for tooling integration.

---

## Global Options

These options are available for all commands:

| Option | Short | Default | Description |
|---|---|---|---|
| `--config <path>` | `-c` | `./engine.xml` | Path to engine configuration file |
| `--verbose` | `-v` | off | Enable verbose logging (debug level) |
| `--json` | | off | Output results as JSON (for tooling/MCP) |
| `--help` | `-h` | | Show help for the current command |
| `--version` | | | Print engine version and exit |

---

## Commands

### `odyssey run`

Launch the engine and run a scene.

**Syntax:**
```bash
odyssey run --scene <path> [options]
```

**Options:**

| Option | Default | Description |
|---|---|---|
| `--scene <path>` | (required) | Path to `.scene.xml` file to load |
| `--validation` | off | Enable Vulkan validation layers |
| `--headless` | off | Run without creating a window (for CI/testing) |
| `--frames <N>` | unlimited | Exit after N frames (useful with `--headless`) |
| `--device <index>` | 0 | Vulkan physical device index |
| `--width <pixels>` | 1280 | Window width |
| `--height <pixels>` | 720 | Window height |
| `--vsync` | on | Enable vertical sync |
| `--no-hot-reload` | off | Disable .nadir file hot-reload |

**Examples:**
```bash
# Run the flocking demo
odyssey run --scene scenes/test_flock.scene.xml

# Run headless for 100 frames with validation
odyssey run --scene scenes/test_flock.scene.xml --headless --frames 100 --validation

# Run at 4K resolution on GPU 1
odyssey run --scene my_scene.scene.xml --width 3840 --height 2160 --device 1

# Run without hot-reload (slightly better performance)
odyssey run --scene my_scene.scene.xml --no-hot-reload
```

**JSON output (with `--json`):**
```json
{
  "status": "running",
  "scene": "scenes/test_flock.scene.xml",
  "device": "NVIDIA GeForce RTX 4090",
  "entity_count": 1000,
  "archetypes": ["boid"],
  "fps": 144.2
}
```

---

### `odyssey compile`

Compile a `.nadir` file to SPIR-V without launching the engine.

**Syntax:**
```bash
odyssey compile --shader <path> [options]
```

**Options:**

| Option | Default | Description |
|---|---|---|
| `--shader <path>` | (required) | Path to `.nadir` file to compile |
| `--output <path>` | `<input>.spv` | Output SPIR-V file path |
| `--include-dir <path>` | `behaviors/lib` | Additional include directory |
| `--optimize` | off | Enable SPIR-V optimization passes |
| `--dump-preamble` | off | Print the injected preamble and exit |

**Examples:**
```bash
# Compile a behavior shader
odyssey compile --shader behaviors/shaders/test_flock.nadir

# Compile with custom output path
odyssey compile --shader my_behavior.nadir --output build/my_behavior.spv

# Compile with optimization
odyssey compile --shader my_behavior.nadir --optimize

# See what preamble gets injected
odyssey compile --shader my_behavior.nadir --dump-preamble
```

**JSON output:**
```json
{
  "status": "success",
  "input": "behaviors/shaders/test_flock.nadir",
  "output": "behaviors/shaders/test_flock.nadir.spv",
  "spirv_size_bytes": 2048,
  "warnings": []
}
```

**Error output:**
```json
{
  "status": "error",
  "input": "behaviors/shaders/broken.nadir",
  "errors": [
    {
      "line": 15,
      "column": 10,
      "message": "undeclared identifier 'undefined_var'"
    }
  ]
}
```

---

### `odyssey validate`

Validate an XML asset file against its XSD schema.

**Syntax:**
```bash
odyssey validate --file <path> [options]
```

**Options:**

| Option | Default | Description |
|---|---|---|
| `--file <path>` | (required) | Path to XML file to validate |
| `--schema <path>` | auto-detected | Path to XSD schema (auto-detected from XML type) |

**Examples:**
```bash
# Validate a scene file
odyssey validate --file scenes/test_flock.scene.xml

# Validate a prefab file with explicit schema
odyssey validate --file prefabs/player.prefab.xml --schema schemas/prefab.xsd
```

**JSON output:**
```json
{
  "status": "valid",
  "file": "scenes/test_flock.scene.xml",
  "schema": "schemas/scene.xsd"
}
```

---

### `odyssey inspect`

Inspect a scene or asset file and print its contents.

**Syntax:**
```bash
odyssey inspect --scene <path> [options]
```

**Options:**

| Option | Default | Description |
|---|---|---|
| `--scene <path>` | (required) | Path to `.scene.xml` file |
| `--entities` | on | List all entities and archetypes |
| `--buffers` | off | Show computed buffer sizes |
| `--behaviors` | off | List referenced `.nadir` files |

**Examples:**
```bash
# List entities in a scene
odyssey inspect --scene scenes/test_flock.scene.xml

# Show buffer memory requirements
odyssey inspect --scene scenes/test_flock.scene.xml --buffers

# List all referenced behavior shaders
odyssey inspect --scene scenes/test_flock.scene.xml --behaviors
```

**Text output:**
```
Scene: test_flock
  World bounds: [-500, -100, -500] to [500, 100, 500]
  Gravity: [0, -9.81, 0]

  Archetypes:
    boid (1000 entities)
      behavior: test_flock.nadir
      stats: health=100 speed=5.0

  Entities:
    player (prefab: player.prefab.xml)
      position: [0, 1, 0]
```

**JSON output:**
```json
{
  "name": "test_flock",
  "world": {
    "bounds": { "min": [-500, -100, -500], "max": [500, 100, 500] },
    "gravity": [0, -9.81, 0]
  },
  "archetypes": [
    {
      "name": "boid",
      "count": 1000,
      "behavior": "test_flock.nadir",
      "stats": { "health": 100, "speed": 5.0 }
    }
  ],
  "entities": [
    {
      "name": "player",
      "prefab": "player.prefab.xml",
      "transform": { "position": [0, 1, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] }
    }
  ]
}
```

---

### `odyssey test`

Run engine tests.

**Syntax:**
```bash
odyssey test [options]
```

**Options:**

| Option | Default | Description |
|---|---|---|
| `--unit` | off | Run unit tests only (no GPU required) |
| `--pipeline` | off | Run pipeline tests only (GPU required) |
| `--shader` | off | Compile-check all `.nadir` files |
| `--filter <pattern>` | all | Run only tests matching the pattern |

If no specific test flag is provided, all tests are run.

**Examples:**
```bash
# Run all tests
odyssey test

# Run only unit tests
odyssey test --unit

# Run only pipeline tests
odyssey test --pipeline

# Compile-check all behavior shaders
odyssey test --shader

# Run tests matching a pattern
odyssey test --filter "BufferLayout*"
```

**JSON output:**
```json
{
  "status": "pass",
  "total": 15,
  "passed": 15,
  "failed": 0,
  "skipped": 0,
  "duration_ms": 342,
  "results": [
    { "name": "DispatchConfig.ComputesCorrectGroupCount", "status": "pass", "duration_ms": 1 },
    { "name": "BufferLayout.ComputesCorrectSizeForArchetype", "status": "pass", "duration_ms": 1 }
  ]
}
```

---

### `odyssey info`

Print engine and system information.

**Syntax:**
```bash
odyssey info [options]
```

**Options:**

| Option | Default | Description |
|---|---|---|
| `--devices` | off | List all Vulkan-capable devices |

**Examples:**
```bash
# Print engine info
odyssey info

# List all GPU devices
odyssey info --devices
```

**Text output:**
```
OdysseyEngine v0.1.0
  Vulkan API: 1.3.268
  Device: NVIDIA GeForce RTX 4090
  Driver: 545.84
  VMA: 3.0.1
  Compute queues: 8
  Max workgroup size: 1024
  Max SSBO size: 2147483647 bytes
```

**JSON output:**
```json
{
  "engine_version": "0.1.0",
  "vulkan_api": "1.3.268",
  "device": {
    "name": "NVIDIA GeForce RTX 4090",
    "driver_version": "545.84",
    "device_type": "discrete",
    "compute_queues": 8,
    "max_workgroup_size": 1024,
    "max_ssbo_size": 2147483647
  },
  "vma_version": "3.0.1"
}
```

---

## Exit Codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | General error (see stderr) |
| 2 | Invalid arguments |
| 3 | File not found |
| 4 | Shader compilation error |
| 5 | Vulkan initialization error |
| 6 | XML validation error |
| 7 | Test failure |

---

## Environment Variables

| Variable | Description |
|---|---|
| `ODYSSEY_CONFIG` | Default path to `engine.xml` (overridden by `--config`) |
| `ODYSSEY_BEHAVIOR_PATH` | Default behavior shader search path |
| `ODYSSEY_LOG_LEVEL` | Default log level (`trace`, `debug`, `info`, `warn`, `error`) |
| `VK_INSTANCE_LAYERS` | Vulkan instance layers (standard Vulkan env var) |

---

## Piping and Scripting

All commands support `--json` for machine-readable output. Combine with standard Unix tools:

```bash
# Get entity count from a scene
odyssey inspect --scene my_scene.scene.xml --json | jq '.archetypes[].count'

# Compile all .nadir files and collect errors
for f in behaviors/shaders/*.nadir; do
    odyssey compile --shader "$f" --json
done | jq 'select(.status == "error")'

# Check if all tests pass in CI
odyssey test --json | jq -e '.status == "pass"'
```
