# /profile

Run the engine with profiling enabled, capture and analyze frame timing data.

## Usage
`/profile [--scene <scene>] [--frames <n>] [--agents <n>]`

## Arguments
- `--scene <scene>`: Scene file to profile (default: `demo/scenes/shooter_arena.scene.xml`). If just a name, resolves to `demo/scenes/<scene>.scene.xml`.
- `--frames <n>`: Number of frames to capture (default: `300`)
- `--agents <n>`: Number of AI agents to simulate (default: `1000`). Overrides scene entity counts for stress testing.

## Prerequisites
- Engine must be built in Release mode. Verify `T:/OdysseyEngine/build/Release/odyssey.exe` exists.
- Profiling in Debug mode gives misleading results. Always use Release.

## Steps

### 1. Build in Release (if needed)
```bash
export VCToolsVersion=14.42.34433
cmake --build T:/OdysseyEngine/build --config Release
```

### 2. Run the benchmark
```bash
cd T:/OdysseyEngine/build && ./Release/odyssey.exe bench <scene> --frames <n> --agents <agents>
```

The engine benchmark mode:
- Runs the specified number of frames without user input
- Captures per-frame timing data
- Outputs a summary with min/max/avg/p99 frame times
- Breaks down GPU compute time per behavior archetype
- Reports CPU orchestration overhead
- Reports memory usage (VRAM and system)

### 3. Capture the output
Save the profiler output for analysis. The engine writes timing data to stdout.

### 4. Analyze the results

**Frame time targets**:
- 60 FPS target: < 16.6ms per frame
- 30 FPS target: < 33.3ms per frame
- Look at p99, not just average (p99 shows worst-case stutters)

**Bottleneck identification**:

| Condition | Diagnosis | Recommendation |
|-----------|-----------|----------------|
| GPU compute > 80% of frame | Shader-bound | Optimize `.nadir` shaders: reduce loop counts, simplify scoring |
| CPU > GPU time | Dispatch-bound | Batch entities by archetype, reduce CPU-GPU sync points |
| Memory > 80% VRAM | Memory-bound | Reduce `max_agents` in `engine.xml`, use smaller buffers |
| p99 >> average | Stutter/spike | Check for GC, reallocation, or pipeline stalls |
| Ally search loops slow | O(n^2) search | Reduce `search_range` in `.nadir` files or use spatial hash |

**Per-archetype breakdown**:
- Compare GPU time for each behavior shader
- Shaders with ally-search loops (e.g., `enemy_pack_hunter`) will scale worse with agent count
- Simple shaders (e.g., `projectile_physics`) should be nearly free

### 5. Report findings
Present:
1. Summary: total frame time, FPS, whether target is met
2. Breakdown: GPU compute vs CPU overhead vs present/swap
3. Per-shader times (if available)
4. Top bottleneck and specific recommendation
5. Comparison with different agent counts if multiple runs were done

## Scaling Tests
To understand how the engine scales, run multiple benchmarks:
```bash
cd T:/OdysseyEngine/build
./Release/odyssey.exe bench demo/scenes/shooter_arena.scene.xml --frames 300 --agents 100
./Release/odyssey.exe bench demo/scenes/shooter_arena.scene.xml --frames 300 --agents 1000
./Release/odyssey.exe bench demo/scenes/shooter_arena.scene.xml --frames 300 --agents 10000
./Release/odyssey.exe bench demo/scenes/shooter_arena.scene.xml --frames 300 --agents 100000
```
This reveals the scaling curve and where the engine hits its performance wall.
