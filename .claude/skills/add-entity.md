# /add-entity

Add an entity to an existing scene XML file.

## Usage
`/add-entity <archetype> [--scene <scene>] [--id <id>] [--position <x y z>] [--count <n>] [--behavior <shader>] [--health <hp>] [--speed <spd>] [--ammo <ammo>]`

## Arguments
- `<archetype>`: Entity archetype name (e.g., `enemy_pack_hunter`, `enemy_ranged`, `civilian`, `static_cover`, `wall`, `multi_arm_gunner`, `player`, `system`)
- `--scene <scene>`: Scene file path or name. If just a name, resolves to `demo/scenes/<scene>.scene.xml`. Default: `shooter_arena`
- `--id <id>`: Unique entity ID within the scene. Auto-generated from archetype if not provided (e.g., `pack_hunter_3`)
- `--position <x y z>`: Spawn position as three floats (default: `0 0 0`)
- `--count <n>`: Number of entities to spawn (default: `1`). When count > 1, a `<spawn_region>` is used instead of a fixed position
- `--behavior <shader>`: Behavior shader name without path (e.g., `enemy_pack_hunter.nadir`). Auto-mapped from archetype if not provided
- `--health <hp>`: Entity health (auto-mapped from archetype if not provided)
- `--speed <spd>`: Entity speed (auto-mapped from archetype if not provided)
- `--ammo <ammo>`: Entity ammo (auto-mapped from archetype if not provided)

## Archetype Defaults

| Archetype | Health | Speed | Ammo | Behavior Shader |
|-----------|--------|-------|------|-----------------|
| player | 100 | 6.0 | 120 | player_input.nadir |
| enemy_pack_hunter | 60 | 7.0 | 30 | enemy_pack_hunter.nadir |
| enemy_ranged | 40 | 3.0 | 50 | enemy_ranged.nadir |
| multi_arm_gunner | 500 | 4.0 | 200 | multi_arm_gunner.nadir |
| civilian | 30 | 5.0 | 0 | civilian_fleeing.nadir |
| static_cover | -- | -- | -- | (none) |
| wall | -- | -- | -- | (none) |

## Steps

### 1. Read the scene file
```bash
cat T:/OdysseyEngine/demo/scenes/<scene>.scene.xml
```

### 2. Generate a unique entity ID
If `--id` is not provided, scan existing entity IDs in the scene and generate a unique one based on the archetype name (e.g., if `hunters_left` and `hunters_right` exist, use `hunters_rear`).

### 3. Build the entity XML block

**Single entity (count = 1)**:
```xml
  <entity id="<id>" archetype="<archetype>">
    <transform position="<x> <y> <z>" rotation="0 0 0 1" scale="1 1 1"/>
    <stats health="<hp>" max_health="<hp>" ammo="<ammo>" speed="<spd>"/>
    <behavior shader="<shader>"/>
  </entity>
```

**Multiple entities (count > 1)**:
```xml
  <entity id="<id>" archetype="<archetype>" count="<n>">
    <spawn_region type="circle" center="<x> <y> <z>" radius="10"/>
    <stats health="<hp>" max_health="<hp>" ammo="<ammo>" speed="<spd>"/>
    <behavior shader="<shader>"/>
  </entity>
```

**Static entities (static_cover, wall)**: Omit `<stats>` and `<behavior>`:
```xml
  <entity id="<id>" archetype="<archetype>">
    <transform position="<x> <y> <z>"/>
  </entity>
```

### 4. Insert into scene
Insert the new entity block before the closing `</scene>` tag, but after any existing entity blocks. Place it before the `game_system` entity if one exists (game_system should remain last).

### 5. Report
Print:
- The entity block that was added
- The updated entity count in the scene
- Suggest running the scene with `/run` to see the changes
