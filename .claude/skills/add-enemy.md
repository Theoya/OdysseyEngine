# /add-enemy

Add enemies to a scene.

## Usage
`/add-enemy <type> <count> [--scene <path>]`

## Arguments
- `type`: Enemy archetype (pack_hunter, ranged, multi_arm_gunner, civilian)
- `count`: Number of enemies to spawn
- `--scene`: Scene file path (default: last opened scene)

## Steps
1. Read the target scene XML
2. Map type to prefab and behavior shader:
   - pack_hunter -> enemy_pack_hunter.nadir
   - ranged -> enemy_ranged.nadir
   - multi_arm_gunner -> multi_arm_gunner.nadir
   - civilian -> civilian_fleeing.nadir
3. Add `<entity>` block with spawn_region, stats from prefab defaults
4. Write updated scene XML
5. If engine is running via MCP, hot-reload the scene
