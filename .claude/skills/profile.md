# /profile

Run benchmark, capture frame profile, and analyze bottlenecks.

## Usage
`/profile [--scene <path>] [--frames <n>] [--agents <n>]`

## Steps
1. Build engine in release mode: `odyssey build --release`
2. Run benchmark: `odyssey bench <scene> --agents <n>` (default 1000 agents, 300 frames)
3. Capture profile output
4. Parse timing data:
   - Total frame time (target: <16.6ms for 60fps)
   - GPU compute time per archetype
   - CPU orchestration overhead
   - Memory usage
5. Identify bottlenecks:
   - If GPU > 80% of frame: shader optimization needed
   - If CPU > GPU: dispatch overhead, consider batching
   - If memory > 80%: reduce entity count or buffer sizes
6. Print analysis with recommendations
