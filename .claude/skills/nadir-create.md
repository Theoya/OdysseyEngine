# /nadir-create

Scaffold a new .nadir behavior shader with all supporting files.

## Usage
`/nadir-create <name> [--description <desc>]`

## Arguments
- `<name>`: Name for the behavior (e.g., `enemy_sniper`, `civilian_worker`). Used as filename and identifiers.
- `--description`: Brief description of what this behavior does (used in file header comment).

## What Gets Created

### 1. Behavior shader: `T:/OdysseyEngine/behaviors/shaders/<name>.nadir`

Use this template:
```glsl
// <name>.nadir -- <description>
#include "scoring.glsl"
#include "steering.glsl"
#include "state_machine.glsl"
#include "blackboard.glsl"
#include "debug.glsl"

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= total_entities) return;

    // Read state
    vec3 pos = positions[idx].xyz;
    float hp = stats[idx].health;
    float max_hp = stats[idx].max_health;
    float spd = stats[idx].speed;
    float ammo = stats[idx].ammo;
    vec3 player_pos = player_position.xyz;
    float dt = delta_time;

    // Persistent state
    uint state = persist[idx].current_state;
    float state_timer = persist[idx].state_timer + dt;
    float cd0 = cooldown_tick(persist[idx].cooldown_0, dt);
    float cd1 = cooldown_tick(persist[idx].cooldown_1, dt);

    // Scores
    float health_norm = score_health(hp, max_hp);
    float damage_norm = score_damage(hp, max_hp);
    float dist_to_player = distance_to(pos, player_pos);
    float proximity = score_proximity(dist_to_player, 30.0);

    // TODO: Implement behavior scoring and state transitions
    // Available states: STATE_PATROL, STATE_COMBAT, STATE_FLEE, STATE_ALERT
    // Use hysteresis_bonus() to prevent state oscillation
    // Use score_max4() for state transition decisions

    uint new_state = state;

    // Reset timer on state change
    if (new_state != state) state_timer = 0.0;

    // TODO: Implement movement per state
    vec3 move_dir = vec3(0.0);
    float move_weight = 0.0;
    float attack_weight = 0.0;

    // Normalize movement
    float move_len = length(move_dir);
    if (move_len > 0.001) move_dir = (move_dir / move_len) * min(move_len, spd);

    // Write outputs
    outputs[idx].move_vector = vec4(move_dir, move_weight);
    outputs[idx].attack_target = vec4(0.0);
    outputs[idx].animation_id = new_state;
    outputs[idx].animation_blend = clamp(move_len / spd, 0.0, 1.0);
    outputs[idx].sound_event = 0u;
    outputs[idx].sound_priority = 0.0;
    outputs[idx].comms_signal = 0.0;
    outputs[idx].comms_urgency = 0.0;

    // Update persistent state
    persist[idx].current_state = new_state;
    persist[idx].state_timer = state_timer;
    persist[idx].cooldown_0 = cd0;
    persist[idx].cooldown_1 = cd1;
    persist[idx].last_decision = new_state;

    // Debug
    debug_state_color(idx, new_state);
}
```

### 2. Prefab: `T:/OdysseyEngine/demo/prefabs/<name>.prefab.xml`

Only create if it does not already exist:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<prefab name="<name>" version="1">
  <components>
    <transform/>
    <stats health="100" max_health="100" ammo="0" stamina="100" speed="5.0"/>
    <behavior shader="<name>.nadir"/>
    <render mesh="demo/materials/<name>.mesh.xml" material="demo/materials/default.mat.xml"/>
    <collider type="capsule" radius="0.4" height="1.8"/>
    <ai_state initial="PATROL"/>
  </components>
</prefab>
```

Adjust stats values based on the entity type:
- Enemies: health 40-100, speed 3-8, ammo as needed
- Civilians: health 20-50, speed 4-6, no ammo
- Bosses: health 200-500, speed 3-5, high ammo

### 3. Verify compilation
After creating the files, compile the shader to check for errors:
```bash
cd T:/OdysseyEngine/build && ./Release/odyssey.exe nadir compile behaviors/shaders/<name>.nadir
```
If the engine executable is not built yet, skip this step and inform the user.

## Available Library Includes
These are the GLSL includes available in `behaviors/lib/`:
- `scoring.glsl` - Score functions: `score_health()`, `score_damage()`, `score_proximity()`, `score_distance()`, `score_max4()`, `hysteresis_bonus()`
- `steering.glsl` - Steering behaviors: `steer_seek()`, `steer_flee()`, `steer_arrive()`, `steer_flock()`
- `spatial.glsl` - Spatial queries: `distance_to()`, `direction_to()`
- `state_machine.glsl` - State constants and transitions: `STATE_PATROL`, `STATE_COMBAT`, `STATE_FLEE`, `STATE_ALERT`
- `blackboard.glsl` - Persistent memory: `bb_store_home_pos()`, `bb_recall_home_pos()`, `bb_store_last_player_pos()`
- `debug.glsl` - Debug visualization: `debug_state_color()`

## Output
Print the paths of all created files and suggest next steps:
1. Implement the behavior scoring logic in the `.nadir` file
2. Optionally create an action sequence with `/create-action-sequence`
3. Add the entity to a scene with `/add-entity`
4. Test the behavior with `/test-behavior` (if available) or `/nadir-compile`
