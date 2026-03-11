# /scene-create

Create a new scene XML file with world settings and basic entities.

## Usage
`/scene-create <name> [--empty] [--with-enemies <type>]`

## Arguments
- `<name>`: Scene name (e.g., `test_arena`, `defense_map`). The file will be created as `demo/scenes/<name>.scene.xml`.
- `--empty`: Create a minimal scene with only world settings and a player spawn (no enemies or cover).
- `--with-enemies <type>`: Pre-populate with a set of enemies of the given type (pack_hunter, ranged, multi_arm_gunner, civilian).

## Steps

### 1. Verify the scene does not already exist
```bash
ls T:/OdysseyEngine/demo/scenes/<name>.scene.xml 2>/dev/null
```
If it exists, warn the user and ask for confirmation before overwriting.

### 2. Create the scene file at `T:/OdysseyEngine/demo/scenes/<name>.scene.xml`

Use this template as the base:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<scene name="<name>" version="1">
  <world>
    <time_scale>1.0</time_scale>
    <gravity>0 -9.81 0</gravity>
  </world>

  <!-- Player spawn -->
  <entity id="player_1" archetype="player">
    <transform position="0 1 0" rotation="0 0 0 1" scale="1 1 1"/>
    <stats health="100" max_health="100" ammo="120"/>
    <behavior shader="player_input.nadir"/>
    <script class="PlayerController"/>
  </entity>

  <!-- Game systems -->
  <entity id="game_system" archetype="system">
    <script class="GameManager"/>
    <script class="HUD"/>
  </entity>
</scene>
```

If `--with-enemies` is specified, add enemy entities between the player and game_system blocks. Use these defaults per type:

**pack_hunter** (adds a squad of 8):
```xml
  <entity id="hunters_squad" archetype="enemy_pack_hunter" count="8">
    <spawn_region type="circle" center="0 0 25" radius="10"/>
    <stats health="60" max_health="60" ammo="30" speed="7.0"/>
    <behavior shader="enemy_pack_hunter.nadir"/>
  </entity>
```

**ranged** (adds 4 snipers):
```xml
  <entity id="snipers" archetype="enemy_ranged" count="4">
    <spawn_region type="circle" center="0 5 40" radius="15"/>
    <stats health="40" max_health="40" ammo="50" speed="3.0"/>
    <behavior shader="enemy_ranged.nadir"/>
  </entity>
```

**multi_arm_gunner** (adds 1 boss):
```xml
  <entity id="boss_gunner" archetype="multi_arm_gunner">
    <transform position="30 1 30" rotation="0 0 0 1" scale="1.5 1.5 1.5"/>
    <stats health="500" max_health="500" ammo="200" speed="4.0"/>
    <behavior shader="multi_arm_gunner.nadir"/>
  </entity>
```

**civilian** (adds 12 civilians):
```xml
  <entity id="civilians" archetype="civilian" count="12">
    <spawn_region type="circle" center="0 0 0" radius="20"/>
    <stats health="30" max_health="30" speed="5.0"/>
    <behavior shader="civilian_fleeing.nadir"/>
  </entity>
```

Unless `--empty` is given, also add basic arena boundaries:
```xml
  <!-- Arena boundaries -->
  <entity id="wall_north" archetype="wall">
    <transform position="0 2.5 50" scale="100 5 1"/>
  </entity>
  <entity id="wall_south" archetype="wall">
    <transform position="0 2.5 -10" scale="100 5 1"/>
  </entity>
  <entity id="wall_east" archetype="wall">
    <transform position="50 2.5 20" scale="1 5 60"/>
  </entity>
  <entity id="wall_west" archetype="wall">
    <transform position="-50 2.5 20" scale="1 5 60"/>
  </entity>
```

### 3. Validate the scene (if engine is built)
```bash
cd T:/OdysseyEngine/build && ./Release/odyssey.exe scene validate demo/scenes/<name>.scene.xml
```

### 4. Report
Print the file path and suggest next steps:
- Add more entities with `/add-entity`
- Run the scene with `/run <name>`
- Validate with `/validate-scene <name>`

## Schema Reference
Scene XML must conform to `T:/OdysseyEngine/schemas/scene.xsd`. Key rules:
- Root element: `<scene name="..." version="1">`
- `<world>` is optional (max 1)
- `<entity>` elements require `id` (unique string) and `archetype` (string)
- `count` attribute on entity spawns multiple instances (requires `<spawn_region>`)
- Child elements of entity: `<transform>`, `<stats>`, `<behavior>`, `<mesh>`, `<script>`, `<pack>` (all optional)
- Available behavior shaders: player_input, enemy_pack_hunter, enemy_ranged, multi_arm_gunner, civilian_fleeing, projectile_physics, test_flock
