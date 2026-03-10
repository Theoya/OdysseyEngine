# /nadir-create

Scaffold a new behavior shader with test and scene registration.

## Usage
`/nadir-create <name>`

## Steps
1. Create `behaviors/shaders/<name>.nadir` from template with standard includes and main()
2. Create `tests/shader/test_<name>.cpp` with basic GPU round-trip test
3. Create `demo/prefabs/<name>.prefab.xml` if it doesn't exist
4. Print confirmation with file paths

## Template (.nadir)
```glsl
// <name>.nadir — [Description]
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

    // TODO: Implement behavior scoring

    // Write outputs
    outputs[idx].move_vector = vec4(0.0, 0.0, 0.0, 0.0);
    outputs[idx].attack_target = vec4(0.0);
    outputs[idx].animation_id = 0u;
    outputs[idx].animation_blend = 0.0;
    outputs[idx].sound_event = 0u;
    outputs[idx].sound_priority = 0.0;
    outputs[idx].comms_signal = 0.0;
    outputs[idx].comms_urgency = 0.0;

    debug_data[idx] = vec4(0.0);
}
```
