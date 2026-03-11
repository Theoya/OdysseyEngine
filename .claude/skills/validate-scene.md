# /validate-scene

Validate a scene XML file against the engine schema and check for common issues.

## Usage
`/validate-scene <scene>`

## Arguments
- `<scene>`: Scene file path or name. If just a name (e.g., `shooter_arena`), resolves to `demo/scenes/shooter_arena.scene.xml`.

## Steps

### 1. Resolve the scene file path
```bash
ls T:/OdysseyEngine/demo/scenes/<scene>.scene.xml
```
If a full path was given, use it directly.

### 2. Validate using the engine CLI (if built)
```bash
cd T:/OdysseyEngine/build && ./Release/odyssey.exe scene validate demo/scenes/<scene>.scene.xml
```

### 3. Manual validation checks
Even if the CLI is not available, perform these checks by reading the scene file:

#### Schema conformance (against `T:/OdysseyEngine/schemas/scene.xsd`)
- [ ] Root element is `<scene>` with `name` and `version` attributes
- [ ] `version` attribute is a positive integer (currently `1`)
- [ ] `<world>` element appears at most once
- [ ] Every `<entity>` has both `id` and `archetype` attributes
- [ ] All entity `id` values are unique within the scene
- [ ] `count` attribute (if present) is a positive integer
- [ ] Child elements of `<entity>` are only: `transform`, `stats`, `behavior`, `mesh`, `script`, `pack`, `spawn_region`

#### Behavioral correctness
- [ ] Every entity with a `<behavior>` element references a `.nadir` file that exists in `T:/OdysseyEngine/behaviors/shaders/`
- [ ] Every entity with `count > 1` has a `<spawn_region>` child element
- [ ] Player entity exists (archetype="player") with `player_input.nadir` behavior
- [ ] Stats values are sensible: `health > 0`, `max_health >= health`, `speed > 0`
- [ ] Transform positions are within reasonable world bounds

#### Reference integrity
- [ ] Prefab references (if any `archetype` matches a `.prefab.xml`) exist in `T:/OdysseyEngine/demo/prefabs/`
- [ ] Mesh references (`<mesh src="...">`) point to existing files
- [ ] Script class references (`<script class="...">`) correspond to registered C++ scripts (GameManager, HUD, PlayerController)
- [ ] Behavior shader references exist in `behaviors/shaders/`

#### Common issues to flag
- **Duplicate entity IDs**: Two entities with the same `id` attribute
- **Missing behavior shader**: `<behavior shader="xxx.nadir"/>` where `xxx.nadir` does not exist
- **Orphaned spawn_region**: `<spawn_region>` on an entity with `count="1"`
- **No game_system entity**: Scene should typically have a system entity with GameManager and HUD scripts
- **Unreachable spawn positions**: Entities spawned outside arena walls
- **Zero health**: Entity with `health="0"` will be immediately dead

### 4. Report
Print a validation report:
- **PASS** or **FAIL** overall status
- List of all checks performed with pass/fail
- For failures: describe the issue, the line in the scene file, and how to fix it
- For warnings (non-fatal issues): list them separately

## Schema Location
- Scene schema: `T:/OdysseyEngine/schemas/scene.xsd`
- Prefab schema: `T:/OdysseyEngine/schemas/prefab.xsd`

## Available Behavior Shaders
These are the valid shader names that can be referenced in `<behavior shader="...">`:
- `player_input.nadir`
- `enemy_pack_hunter.nadir`
- `enemy_ranged.nadir`
- `multi_arm_gunner.nadir`
- `civilian_fleeing.nadir`
- `projectile_physics.nadir`
- `test_flock.nadir`

## Available Prefabs
These are the valid archetypes with matching prefab definitions:
- `player` -> `demo/prefabs/player.prefab.xml`
- `enemy_pack_hunter` -> `demo/prefabs/enemy_pack_hunter.prefab.xml`
- `enemy_ranged` -> `demo/prefabs/enemy_ranged.prefab.xml`
- `multi_arm_gunner` -> `demo/prefabs/multi_arm_gunner.prefab.xml`
- `civilian` -> `demo/prefabs/civilian.prefab.xml`
- `projectile` -> `demo/prefabs/projectile.prefab.xml`
