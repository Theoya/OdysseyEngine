# Nadir Behavior Authoring Guide

Nadir is OdysseyEngine's GPU-parallel behavior system. This guide covers everything you need to write, test, and debug `.nadir` behavior shaders.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [The .nadir File Format](#the-nadir-file-format)
3. [Available Buffers](#available-buffers)
4. [Scoring Patterns](#scoring-patterns)
5. [Steering Behaviors](#steering-behaviors)
6. [State Machines in Shaders](#state-machines-in-shaders)
7. [Multi-Action Outputs](#multi-action-outputs)
8. [Best Practices](#best-practices)
9. [Examples](#examples)
10. [Debugging](#debugging)

---

## Getting Started

### Creating Your First .nadir File

A `.nadir` file is a GLSL compute shader with a simplified authoring surface. The Nadir system injects a preamble that provides buffer bindings, utility functions, and the compute shader boilerplate. You write only the behavior logic.

Create a new file at `demo/behaviors/my_behavior.nadir`:

```glsl
#include "scoring.glsl"
#include "steering.glsl"

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;

    // Read this entity's state
    vec3 my_pos  = entity_transforms[idx].position.xyz;
    float health = entity_stats[idx].health;

    // Simple behavior: move toward origin
    vec3 to_origin = -my_pos;
    float dist = length(to_origin);

    behavior_output[idx].move_vector = normalize(to_origin);
    behavior_output[idx].move_weight = score_linear(dist, 100.0, 0.0);
}
```

### Compiling and Testing

```bash
# Compile without launching the engine
odyssey compile --shader demo/behaviors/my_behavior.nadir

# Test all shaders compile successfully
odyssey test --shader

# Run in a scene
odyssey run --scene scenes/my_scene.scene.xml
```

### Assigning to an Archetype

Reference the behavior in a scene file:

```xml
<archetype name="wanderer" count="500" behavior="my_behavior.nadir">
    <spawn>
        <region type="box" min="-100 0 -100" max="100 0 100" />
    </spawn>
    <stats health="100" speed="3.0" />
</archetype>
```

---

## The .nadir File Format

A `.nadir` file is GLSL with a Nadir-injected preamble. You write the `main()` function; the system provides everything else.

### What the Preamble Provides

The following is automatically injected before your code:

```glsl
#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// Buffer bindings (see "Available Buffers" section)
layout(std430, binding = 0) readonly buffer EntityTransformBuffer { ... };
layout(std430, binding = 1) readonly buffer EntityStatsBuffer { ... };
layout(std430, binding = 2) readonly buffer SpatialGridBuffer { ... };
layout(std430, binding = 3) readonly buffer WorldStateBuffer { ... };
layout(std430, binding = 4) buffer AgentPersistStateBuffer { ... };
layout(std430, binding = 5) writeonly buffer BehaviorOutputBuffer { ... };
layout(std430, binding = 6) writeonly buffer DebugOutputBuffer { ... };
```

### What You Write

```glsl
// Optional: #include behavior library files
#include "scoring.glsl"
#include "steering.glsl"

// Required: main function
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;

    // Your behavior logic here
    // Read from buffers 0-4
    // Write to buffers 4-6
}
```

### #include Resolution

`#include` directives are resolved against the behavior library path (default: `behaviors/lib/`). The Nadir compiler uses shaderc's includer interface to resolve these at compile time.

```glsl
#include "scoring.glsl"     // resolves to behaviors/lib/scoring.glsl
#include "steering.glsl"    // resolves to behaviors/lib/steering.glsl
```

---

## Available Buffers

### Buffer 0: EntityTransforms (Read-Only)

Per-entity position data. Updated by the CPU each frame after applying movement from the previous frame's behavior output.

```glsl
struct EntityTransform {
    vec4 position;      // xyz = world position, w = unused
};
// Access: entity_transforms[idx].position
```

### Buffer 1: EntityStats (Read-Only)

Per-entity gameplay statistics. Updated by the CPU as game events occur.

```glsl
struct EntityStat {
    float health;       // 0.0 = dead, 100.0 = full
    float ammo;         // current ammo count
    float stamina;      // 0.0 = exhausted, 100.0 = full
    float speed;        // max movement speed
    float armor;        // damage reduction factor
    float threat_level; // CPU-computed threat assessment
    float _pad0;
    float _pad1;
};
// Access: entity_stats[idx].health
```

### Buffer 2: SpatialGrid (Read-Only)

Spatial partitioning grid for neighbor queries. The CPU updates this each frame based on entity positions.

```glsl
struct SpatialCell {
    uint entity_count;      // number of entities in this cell
    uint entity_offset;     // offset into entity index list
    float _pad0;
    float _pad1;
};
// Access: spatial_grid[cell_index].entity_count
```

### Buffer 3: WorldState (Read-Only)

Global state shared across all entities. Updated once per frame by the CPU.

```glsl
struct WorldState {
    float time;             // total elapsed time (seconds)
    float delta_time;       // time since last frame (seconds)
    uint entity_count;      // total entities in this archetype
    uint frame;             // frame counter
    vec4 player_position;   // player world position
    vec4 objective_position;// current objective position
};
// Access: world_state.time, world_state.player_position, etc.
```

### Buffer 4: AgentPersistState (Read/Write)

Per-entity persistent state that survives across frames. Use this for state machines, cooldown timers, and memory.

```glsl
struct AgentPersistState {
    uint state_id;          // current FSM state
    uint previous_state;    // previous FSM state (for transition detection)
    float timers[4];        // general-purpose timers
    float memory[8];        // general-purpose persistent memory
};
// Access: agent_state[idx].state_id = STATE_ATTACK;
//         agent_state[idx].timers[0] += world_state.delta_time;
```

### Buffer 5: BehaviorOutput (Write-Only)

Per-entity behavior output consumed by downstream systems. Write your behavior decisions here.

```glsl
struct BehaviorOutput {
    vec4 move_vector;       // xyz = desired movement direction, w = unused
    float move_weight;      // 0.0-1.0, confidence in movement
    uint attack_target;     // entity ID to attack (0 = none)
    float attack_weight;    // 0.0-1.0, confidence in attack
    uint animation_id;      // animation to play
    float animation_blend;  // blend factor for animation transitions
    vec4 comms_data;        // data broadcast to nearby allies
    float comms_weight;     // 0.0-1.0, whether to broadcast
    float _pad0;
    float _pad1;
    float _pad2;
};
// Access: behavior_output[idx].move_vector = vec4(dir, 0.0);
```

### Buffer 6: DebugOutput (Write-Only)

Per-entity debug data for visualization overlays.

```glsl
struct DebugOutput {
    vec4 debug_color;       // RGBA color for debug visualization
};
// Access: debug_output[idx].debug_color = vec4(1.0, 0.0, 0.0, 1.0); // red
```

---

## Scoring Patterns

The scoring library (`behaviors/lib/scoring.glsl`) provides utility curves for converting game state into behavior weights. All scoring functions return values in the range [0.0, 1.0].

### Linear Score

Maps a value linearly from `high` (score 1.0) to `low` (score 0.0):

```glsl
float score_linear(float value, float low, float high);
```

**Examples:**
```glsl
// Distance scoring: closer = higher score
float chase_score = score_linear(distance, 50.0, 0.0);

// Health scoring: more health = higher score
float confidence = score_linear(health, 0.0, 100.0);
```

### Inverse Score

High score when value is near `low`, drops off toward `high`:

```glsl
float score_inverse(float value, float low, float high);
```

**Examples:**
```glsl
// Flee when health is low
float flee_score = score_inverse(health, 0.0, 40.0);

// Urgency when ammo is low
float reload_score = score_inverse(ammo, 0.0, 5.0);
```

### Bell Curve Score

Peaks at `center`, falls off symmetrically with given `width`:

```glsl
float score_bell(float value, float center, float width);
```

**Examples:**
```glsl
// Prefer medium range combat (not too close, not too far)
float engage_score = score_bell(distance, 30.0, 15.0);

// Prefer moderate speed
float cruise_score = score_bell(current_speed, 5.0, 2.0);
```

### Step Score

Returns 1.0 if value >= threshold, 0.0 otherwise. Uses smoothstep for GPU-friendly interpolation:

```glsl
float score_step(float value, float threshold);
```

**Examples:**
```glsl
// Only attack if we have ammo
float can_attack = score_step(ammo, 1.0);

// Only flee if there are too many enemies
float overwhelmed = score_step(float(nearby_enemies), 3.0);
```

### Cooldown Score

Returns 1.0 when timer exceeds cooldown duration, 0.0 otherwise:

```glsl
float score_cooldown(float timer, float cooldown_duration);
```

**Examples:**
```glsl
// Fire only when weapon cooldown is ready
float fire_ready = score_cooldown(agent_state[idx].timers[0], 2.0);

// Periodically call for reinforcements
float call_ready = score_cooldown(agent_state[idx].timers[1], 10.0);
```

### Combining Scores

Multiply scores to require multiple conditions (AND logic). Add scores for optional boosting (OR-like logic). Use `max()` for highest-priority selection.

```glsl
// AND: attack only if we have ammo AND enemy is close AND weapon is ready
float attack_score = score_step(ammo, 1.0)
                   * score_linear(distance, 50.0, 5.0)
                   * score_cooldown(timer, 2.0);

// Boosted: base patrol score, boosted if we're healthy
float patrol_score = 0.2 + 0.3 * score_linear(health, 50.0, 100.0);
```

---

## Steering Behaviors

The steering library (`behaviors/lib/steering.glsl`) provides standard steering behaviors that return direction vectors.

### Seek

Move directly toward a target:

```glsl
vec3 seek(vec3 current_pos, vec3 target_pos);
```

```glsl
vec3 to_player = seek(my_pos, world_state.player_position.xyz);
```

### Flee

Move directly away from a threat:

```glsl
vec3 flee(vec3 current_pos, vec3 threat_pos);
```

```glsl
vec3 away = flee(my_pos, world_state.player_position.xyz);
```

### Arrive

Move toward target, slowing down within a deceleration radius:

```glsl
vec3 arrive(vec3 current_pos, vec3 target_pos, float decel_radius);
```

```glsl
vec3 approach = arrive(my_pos, cover_pos, 10.0);
```

### Wander

Generate a pseudo-random wander direction based on entity index and time:

```glsl
vec3 wander(uint entity_index, float time, float radius);
```

```glsl
vec3 drift = wander(idx, world_state.time, 5.0);
```

### Flocking

Reynolds flocking with separation, alignment, and cohesion. Requires neighbor data from the spatial grid.

```glsl
struct FlockResult {
    vec3 separation;    // steer away from nearby neighbors
    vec3 alignment;     // match heading of nearby neighbors
    vec3 cohesion;      // steer toward center of nearby neighbors
};

FlockResult compute_flock(
    uint idx,
    vec3 my_pos,
    float separation_radius,
    float neighbor_radius
);
```

```glsl
FlockResult flock = compute_flock(idx, my_pos, 5.0, 20.0);
vec3 flock_dir = flock.separation * 1.5
               + flock.alignment * 1.0
               + flock.cohesion * 1.0;
```

### Obstacle Avoidance

Cast a ray forward and return an avoidance vector if an obstacle is detected:

```glsl
vec3 avoid_obstacles(vec3 pos, vec3 forward, float look_ahead);
```

```glsl
vec3 avoidance = avoid_obstacles(my_pos, my_forward, 15.0);
```

---

## State Machines in Shaders

The `AgentPersistState` buffer (binding 4) provides persistent storage for implementing finite state machines within Nadir shaders.

### Defining States

Use constants for state IDs:

```glsl
const uint STATE_IDLE    = 0;
const uint STATE_PATROL  = 1;
const uint STATE_CHASE   = 2;
const uint STATE_ATTACK  = 3;
const uint STATE_FLEE    = 4;
const uint STATE_DEAD    = 5;
```

### State Transitions

Score each possible transition and let the highest score win:

```glsl
uint current = agent_state[idx].state_id;

// Compute transition scores based on current state and world
float to_chase  = score_linear(distance, 40.0, 10.0) * score_step(health, 20.0);
float to_flee   = score_inverse(health, 0.0, 25.0) * score_step(float(nearby_enemies), 2.0);
float to_attack = score_linear(distance, 10.0, 0.0) * score_step(ammo, 1.0);
float to_patrol = 0.15; // baseline

// Find max score
float max_score = max(max(to_chase, to_flee), max(to_attack, to_patrol));

// Transition (with hysteresis: must beat current by margin)
uint new_state = current;
float hysteresis = 0.1;

if (to_chase  >= max_score - 0.001 && to_chase  > hysteresis) new_state = STATE_CHASE;
if (to_flee   >= max_score - 0.001 && to_flee   > hysteresis) new_state = STATE_FLEE;
if (to_attack >= max_score - 0.001 && to_attack > hysteresis) new_state = STATE_ATTACK;
if (to_patrol >= max_score - 0.001 && max_score < 0.2)        new_state = STATE_PATROL;

// Record transition
agent_state[idx].previous_state = current;
agent_state[idx].state_id = new_state;
```

### Timers

Use the `timers` array for cooldowns, duration tracking, and periodic events:

```glsl
// Timer 0: time in current state
if (agent_state[idx].state_id != agent_state[idx].previous_state) {
    agent_state[idx].timers[0] = 0.0; // reset on state change
} else {
    agent_state[idx].timers[0] += world_state.delta_time;
}

// Timer 1: weapon cooldown
agent_state[idx].timers[1] += world_state.delta_time;
if (should_fire) {
    agent_state[idx].timers[1] = 0.0; // reset cooldown
}

// Timer 2: periodic behavior (call for help every 10s)
agent_state[idx].timers[2] += world_state.delta_time;
if (agent_state[idx].timers[2] > 10.0) {
    agent_state[idx].timers[2] = 0.0;
    // trigger comms
}
```

### Memory

Use the `memory` array for persistent values that influence behavior over time:

```glsl
// memory[0]: last known player position X
// memory[1]: last known player position Y
// memory[2]: last known player position Z
if (can_see_player) {
    agent_state[idx].memory[0] = world_state.player_position.x;
    agent_state[idx].memory[1] = world_state.player_position.y;
    agent_state[idx].memory[2] = world_state.player_position.z;
}

vec3 last_known = vec3(
    agent_state[idx].memory[0],
    agent_state[idx].memory[1],
    agent_state[idx].memory[2]
);

// memory[3]: kill count this wave (for aggression scaling)
// memory[4]: damage taken recently (for flee threshold)
```

---

## Multi-Action Outputs

A key advantage of Nadir over behavior trees is that entities can output multiple simultaneous actions with independent weights. Downstream systems handle each action channel independently.

### Action Channels

| Channel | Fields | Downstream System |
|---|---|---|
| Movement | `move_vector`, `move_weight` | Physics / transform update |
| Attack | `attack_target`, `attack_weight` | Combat system |
| Animation | `animation_id`, `animation_blend` | Animation system |
| Communication | `comms_data`, `comms_weight` | AI communication system |

### Writing Multiple Actions

```glsl
// Movement: blend of multiple steering behaviors
vec3 chase_dir  = seek(my_pos, target_pos);
vec3 avoid_dir  = avoid_obstacles(my_pos, forward, 15.0);
vec3 flock_dir  = flock.separation * 1.5 + flock.alignment + flock.cohesion;

behavior_output[idx].move_vector = vec4(
    normalize(chase_dir * 0.6 + avoid_dir * 0.3 + flock_dir * 0.1),
    0.0
);
behavior_output[idx].move_weight = max(chase_score, flee_score);

// Attack: independent of movement
behavior_output[idx].attack_target = nearest_enemy;
behavior_output[idx].attack_weight = attack_score * score_cooldown(weapon_timer, 1.5);

// Animation: based on dominant behavior
behavior_output[idx].animation_id = (attack_score > flee_score) ? ANIM_ATTACK : ANIM_RUN;
behavior_output[idx].animation_blend = abs(attack_score - flee_score);

// Communication: broadcast position when hurt
behavior_output[idx].comms_data = vec4(my_pos, health);
behavior_output[idx].comms_weight = score_inverse(health, 0.0, 50.0) * 0.8;
```

### Multi-Arm Example

For an entity with multiple independent limbs (e.g., a 4-armed gunner), encode per-arm data in the output:

```glsl
// Use comms_data channels for per-arm weapon targets
// comms_data.x = arm 0 target angle
// comms_data.y = arm 1 target angle
// comms_data.z = arm 2 target angle
// comms_data.w = arm 3 target angle

// Score each arm's target independently
for (int arm = 0; arm < 4; arm++) {
    float angle = atan(enemies[arm].z - my_pos.z, enemies[arm].x - my_pos.x);
    float arm_score = score_linear(length(enemies[arm] - my_pos), 50.0, 5.0);
    // Pack into comms_data
    behavior_output[idx].comms_data[arm] = angle * arm_score;
}
```

---

## Best Practices

### Avoid Branching

GPU threads in a workgroup execute in lockstep (SIMD). When threads diverge on a branch, both paths execute and results are masked. This wastes GPU cycles.

**Bad -- branching:**
```glsl
// Thread divergence: some threads take if, some take else
if (health < 30.0) {
    behavior_output[idx].move_vector = vec4(flee_dir, 0.0);
} else if (distance < 10.0) {
    behavior_output[idx].move_vector = vec4(attack_dir, 0.0);
} else {
    behavior_output[idx].move_vector = vec4(patrol_dir, 0.0);
}
```

**Good -- branchless scoring:**
```glsl
// All threads execute the same instructions, no divergence
float flee_w   = score_inverse(health, 0.0, 30.0);
float attack_w = score_linear(distance, 10.0, 0.0) * score_step(health, 30.0);
float patrol_w = 0.1;

vec3 blended = flee_dir * flee_w + attack_dir * attack_w + patrol_dir * patrol_w;
behavior_output[idx].move_vector = vec4(normalize(blended), 0.0);
```

### Use Math Instead of Logic

Replace boolean logic with smooth mathematical functions:

```glsl
// Bad: boolean check
float can_attack = (ammo > 0 && distance < 20.0) ? 1.0 : 0.0;

// Good: smooth scoring
float can_attack = score_step(ammo, 1.0) * score_linear(distance, 30.0, 5.0);
```

### Normalize Scores

Keep all scores in [0.0, 1.0] so they compose predictably:

```glsl
// If combining many scores, normalize the result
float total = attack_w + flee_w + patrol_w + comms_w;
if (total > 0.0) {
    attack_w /= total;
    flee_w   /= total;
    patrol_w /= total;
    comms_w  /= total;
}
```

### Early-Out for Invalid Entities

Always guard against out-of-bounds entity indices:

```glsl
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;  // REQUIRED
    // ...
}
```

### Keep Shaders Focused

Each `.nadir` file should define the behavior for one archetype. If behaviors share logic, extract it to a library file in `behaviors/lib/` and `#include` it.

---

## Examples

### Flocking (Reynolds Boids)

```glsl
// demo/behaviors/test_flock.nadir
#include "scoring.glsl"
#include "steering.glsl"

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;

    vec3 my_pos = entity_transforms[idx].position.xyz;

    // Compute flocking forces
    FlockResult flock = compute_flock(idx, my_pos, 5.0, 25.0);

    // Blend with weights
    vec3 dir = flock.separation * 2.0
             + flock.alignment * 1.0
             + flock.cohesion * 1.0;

    // Soft boundary: steer back when far from origin
    float dist_from_center = length(my_pos);
    vec3 to_center = -normalize(my_pos);
    float boundary_w = smoothstep(80.0, 120.0, dist_from_center);
    dir += to_center * boundary_w * 3.0;

    behavior_output[idx].move_vector = vec4(normalize(dir), 0.0);
    behavior_output[idx].move_weight = 1.0;

    // Debug: color by speed
    debug_output[idx].debug_color = vec4(0.2, 0.6, 1.0, 1.0);
}
```

### Pack Hunting

```glsl
// demo/behaviors/pack_hunter.nadir
#include "scoring.glsl"
#include "steering.glsl"

const uint STATE_STALK  = 0;
const uint STATE_CIRCLE = 1;
const uint STATE_LUNGE  = 2;
const uint STATE_FLEE   = 3;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;

    vec3 my_pos   = entity_transforms[idx].position.xyz;
    vec3 prey_pos = world_state.player_position.xyz;
    float health  = entity_stats[idx].health;
    float dist    = length(prey_pos - my_pos);

    // Count nearby packmates (simplified, using spatial grid in production)
    // For this example, use a proxy from memory
    float pack_size = agent_state[idx].memory[0];

    // Score transitions
    float stalk_s  = score_bell(dist, 40.0, 15.0) * score_step(health, 40.0);
    float circle_s = score_bell(dist, 25.0, 10.0) * score_step(pack_size, 3.0);
    float lunge_s  = score_linear(dist, 15.0, 3.0) * score_step(pack_size, 3.0)
                   * score_step(health, 50.0);
    float flee_s   = score_inverse(health, 0.0, 30.0);

    float max_s = max(max(stalk_s, circle_s), max(lunge_s, flee_s));
    uint new_state = STATE_STALK;
    if (circle_s >= max_s - 0.001) new_state = STATE_CIRCLE;
    if (lunge_s  >= max_s - 0.001) new_state = STATE_LUNGE;
    if (flee_s   >= max_s - 0.001) new_state = STATE_FLEE;

    agent_state[idx].state_id = new_state;

    // Compute movement based on state scores
    vec3 stalk_dir  = seek(my_pos, prey_pos) * 0.3;     // slow approach
    vec3 circle_dir = cross(normalize(prey_pos - my_pos), vec3(0,1,0)); // orbit
    vec3 lunge_dir  = seek(my_pos, prey_pos);            // full speed
    vec3 flee_dir   = flee(my_pos, prey_pos);

    vec3 move = stalk_dir * stalk_s + circle_dir * circle_s
              + lunge_dir * lunge_s + flee_dir * flee_s;

    behavior_output[idx].move_vector = vec4(normalize(move), 0.0);
    behavior_output[idx].move_weight = max_s;
    behavior_output[idx].attack_target = (new_state == STATE_LUNGE) ? 0 : 0xFFFFFFFF;
    behavior_output[idx].attack_weight = lunge_s;

    // Comms: broadcast position to pack
    behavior_output[idx].comms_data = vec4(my_pos, health);
    behavior_output[idx].comms_weight = 0.5;

    // Debug: color by state
    vec4 colors[4] = vec4[4](
        vec4(0.5, 0.5, 0.5, 1.0),  // stalk: gray
        vec4(1.0, 1.0, 0.0, 1.0),  // circle: yellow
        vec4(1.0, 0.0, 0.0, 1.0),  // lunge: red
        vec4(0.0, 0.0, 1.0, 1.0)   // flee: blue
    );
    debug_output[idx].debug_color = colors[new_state];
}
```

### Ranged Combat

```glsl
// demo/behaviors/ranged_soldier.nadir
#include "scoring.glsl"
#include "steering.glsl"

const uint STATE_ADVANCE  = 0;
const uint STATE_HOLD     = 1;
const uint STATE_RETREAT  = 2;
const uint STATE_RELOAD   = 3;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;

    vec3 my_pos = entity_transforms[idx].position.xyz;
    vec3 target = world_state.player_position.xyz;
    float health = entity_stats[idx].health;
    float ammo   = entity_stats[idx].ammo;
    float dist   = length(target - my_pos);

    // Weapon timer
    agent_state[idx].timers[0] += world_state.delta_time;
    float weapon_ready = score_cooldown(agent_state[idx].timers[0], 0.5);

    // Scoring
    float advance_s = score_linear(dist, 50.0, 25.0) * score_step(health, 40.0);
    float hold_s    = score_bell(dist, 30.0, 10.0) * score_step(ammo, 1.0);
    float retreat_s = score_inverse(health, 0.0, 35.0) + score_inverse(ammo, 0.0, 3.0) * 0.5;
    float reload_s  = score_inverse(ammo, 0.0, 2.0) * score_linear(dist, 20.0, 40.0);

    // Movement
    vec3 advance_dir = seek(my_pos, target);
    vec3 hold_dir    = vec3(0.0); // stay put
    vec3 retreat_dir = flee(my_pos, target);

    vec3 move = advance_dir * advance_s + hold_dir * hold_s + retreat_dir * retreat_s;
    behavior_output[idx].move_vector = vec4(normalize(move), 0.0);
    behavior_output[idx].move_weight = max(advance_s, retreat_s);

    // Attack: fire if in range, have ammo, and weapon is ready
    float fire_score = score_bell(dist, 30.0, 15.0) * score_step(ammo, 1.0) * weapon_ready;
    behavior_output[idx].attack_target = 0; // player entity
    behavior_output[idx].attack_weight = fire_score;

    // Reset weapon timer on fire
    if (fire_score > 0.7) {
        agent_state[idx].timers[0] = 0.0;
    }

    // Animation
    uint anim = (fire_score > 0.7) ? 3 : // firing
                (retreat_s > advance_s) ? 2 : // running back
                (advance_s > 0.3) ? 1 : // advancing
                0; // idle
    behavior_output[idx].animation_id = anim;
}
```

### Multi-Arm Gunner

```glsl
// demo/behaviors/multi_arm_gunner.nadir
// Demonstrates Nadir's multi-action advantage:
// 4 arms independently score and engage targets simultaneously.
// This is impossible with a behavior tree that picks one action per tick.

#include "scoring.glsl"
#include "steering.glsl"

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= world_state.entity_count) return;

    vec3 my_pos = entity_transforms[idx].position.xyz;
    float health = entity_stats[idx].health;

    // Per-arm cooldown timers (timers 0-3)
    for (int arm = 0; arm < 4; arm++) {
        agent_state[idx].timers[arm] += world_state.delta_time;
    }

    // Movement: hold position or retreat based on health
    float hold_score = score_linear(health, 30.0, 100.0);
    float retreat_score = score_inverse(health, 0.0, 30.0);
    vec3 retreat_dir = flee(my_pos, world_state.player_position.xyz);

    behavior_output[idx].move_vector = vec4(retreat_dir * retreat_score, 0.0);
    behavior_output[idx].move_weight = retreat_score;

    // Primary target
    behavior_output[idx].attack_target = 0;
    behavior_output[idx].attack_weight = score_cooldown(agent_state[idx].timers[0], 0.3);

    // Arms 0-3: independent targeting via comms_data channels
    // Each channel = angle to aim that arm, weighted by score
    vec3 to_player = normalize(world_state.player_position.xyz - my_pos);
    float base_angle = atan(to_player.z, to_player.x);

    // Arm 0: direct fire at player
    float arm0_score = score_cooldown(agent_state[idx].timers[0], 0.3);
    behavior_output[idx].comms_data.x = base_angle * arm0_score;

    // Arm 1: lead the target (aim ahead)
    float arm1_score = score_cooldown(agent_state[idx].timers[1], 0.5);
    behavior_output[idx].comms_data.y = (base_angle + 0.15) * arm1_score;

    // Arm 2: suppressive fire (sweep pattern)
    float arm2_score = score_cooldown(agent_state[idx].timers[2], 0.8);
    float sweep = sin(world_state.time * 2.0) * 0.5;
    behavior_output[idx].comms_data.z = (base_angle + sweep) * arm2_score;

    // Arm 3: defensive (aim at nearest threat)
    float arm3_score = score_cooldown(agent_state[idx].timers[3], 1.0)
                     * score_inverse(health, 0.0, 50.0);
    behavior_output[idx].comms_data.w = (base_angle + 3.14159) * arm3_score;

    // Reset cooldowns on fire
    if (arm0_score > 0.9) agent_state[idx].timers[0] = 0.0;
    if (arm1_score > 0.9) agent_state[idx].timers[1] = 0.0;
    if (arm2_score > 0.9) agent_state[idx].timers[2] = 0.0;
    if (arm3_score > 0.9) agent_state[idx].timers[3] = 0.0;

    // Animation: multi-arm firing
    behavior_output[idx].animation_id = 10; // multi-arm fire animation

    // Debug: red intensity = number of arms currently firing
    float arms_active = arm0_score + arm1_score + arm2_score + arm3_score;
    debug_output[idx].debug_color = vec4(arms_active / 4.0, 0.0, 0.0, 1.0);
}
```

---

## Debugging

### Debug Output Buffer

Write to `debug_output[idx].debug_color` to visualize behavior state. The engine renders debug overlays using this data.

```glsl
// Color by dominant behavior
if (attack_score > flee_score && attack_score > patrol_score) {
    debug_output[idx].debug_color = vec4(1.0, 0.0, 0.0, 1.0); // red: attacking
} else if (flee_score > patrol_score) {
    debug_output[idx].debug_color = vec4(0.0, 0.0, 1.0, 1.0); // blue: fleeing
} else {
    debug_output[idx].debug_color = vec4(0.0, 1.0, 0.0, 1.0); // green: patrolling
}

// Color by health gradient
debug_output[idx].debug_color = vec4(
    1.0 - health / 100.0,  // red when low health
    health / 100.0,         // green when high health
    0.0,
    1.0
);

// Color by state machine state
vec4 state_colors[4] = vec4[4](
    vec4(0.0, 1.0, 0.0, 1.0),  // idle: green
    vec4(1.0, 1.0, 0.0, 1.0),  // patrol: yellow
    vec4(1.0, 0.5, 0.0, 1.0),  // chase: orange
    vec4(1.0, 0.0, 0.0, 1.0)   // attack: red
);
debug_output[idx].debug_color = state_colors[agent_state[idx].state_id];
```

### CLI Debugging

```bash
# Run with verbose logging to see Nadir dispatch details
odyssey run --scene my_scene.scene.xml -v

# Compile a shader and see warnings
odyssey compile --shader my_behavior.nadir -v

# Run with validation layers for GPU debugging
odyssey run --scene my_scene.scene.xml --validation
```

### Common Issues

| Symptom | Likely Cause | Fix |
|---|---|---|
| Entities don't move | `move_weight` is 0.0 | Ensure scoring produces non-zero weights |
| All entities move identically | Not using `gl_GlobalInvocationID.x` | Use `idx` for per-entity state reads |
| Entities jitter | Competing high-weight behaviors | Normalize scores or add hysteresis |
| Shader won't compile | Missing `#include` | Ensure library files exist in `behaviors/lib/` |
| State machine oscillates | No hysteresis on transitions | Add a margin before state change triggers |
| GPU hang | Infinite loop in shader | Shaders must terminate; avoid unbounded loops |
