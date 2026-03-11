# /create-action-sequence

Create a new .actions.xml file defining serial action sequences for an entity archetype.

## Usage
`/create-action-sequence <archetype> [--sequences <list>]`

## Arguments
- `<archetype>`: The entity archetype name (e.g., `enemy_ranged`, `civilian`). The file will be created as `demo/actions/<archetype>.actions.xml`.
- `--sequences <list>`: Comma-separated list of sequence names to scaffold (e.g., `patrol,death,taunt`). If not provided, generates default sequences based on archetype type.

## How Action Sequences Work

Action sequences are **serial, CPU-side** sequences triggered by the GPU-side Nadir behavior shader. The shader writes a non-zero `action_request` value into the `BehaviorOutput` buffer, and the CPU-side `ActionSystem` reads that field each frame to execute the matching sequence.

Each sequence has:
- `name`: Human-readable name
- `id`: Integer ID that matches the shader's `action_request` output
- `loop`: Optional boolean (default `false`). If `true`, the sequence restarts after completing.

## Available Action Commands

| Command | Attributes | Description |
|---------|-----------|-------------|
| `<move_to>` | `position="x y z"` | Move entity to world position |
| `<wait>` | `duration="seconds"` | Pause for a duration |
| `<play_anim>` | `id="N" duration="seconds"` | Play animation by ID |
| `<play_sound>` | `name="sound_name"` | Play a named sound effect |
| `<spawn>` | `prefab="name" offset="x y z"` | Spawn a prefab relative to entity |
| `<destroy_self>` | (none) | Remove the entity from the scene |
| `<look_at>` | `target="entity_id"` or `target="player"` | Face toward a target |
| `<set_state>` | `key="N" value="F"` | Write to persistent memory slot |
| `<emit_signal>` | `value="F"` | Broadcast a comms signal to nearby entities |

## Steps

### 1. Check if the file already exists
```bash
ls T:/OdysseyEngine/demo/actions/<archetype>.actions.xml 2>/dev/null
```
If it exists, warn the user and ask before overwriting.

### 2. Determine default sequences based on archetype

**Enemy archetypes** (pack_hunter, ranged, multi_arm_gunner, etc.):
- `patrol_route` (id=1, loop=true) - Patrol waypoint loop
- `death_sequence` (id=2) - Death animation, loot drop, destroy
- `taunt` (id=3) - Face player, play taunt, emit signal
- `regroup` (id=4) - Rally allies, move to point, wait

**Civilian archetypes**:
- `wander` (id=1, loop=true) - Idle wandering path
- `death_sequence` (id=2) - Death animation, destroy
- `panic_flee` (id=3) - Flee animation, run to exit
- `cower` (id=4) - Cower in place animation

**Boss archetypes** (multi_arm_gunner, etc.):
- `entrance` (id=1) - Boss entrance cinematic
- `death_sequence` (id=2) - Extended death with explosion
- `phase_transition` (id=3) - Visual effect for phase change
- `rage_mode` (id=4) - Rage activation animation

### 3. Create the actions file at `T:/OdysseyEngine/demo/actions/<archetype>.actions.xml`

Template:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!--
  <archetype>.actions.xml
  Serial action sequences for the <archetype> archetype.

  The Nadir shader (<archetype>.nadir) triggers these by writing
  a non-zero action_request into the BehaviorOutput buffer. The CPU-side
  ActionSystem reads that field each frame and executes the matching sequence.

  Sequence IDs:
    1 = <name_1>
    2 = <name_2>
    ...
-->
<actions archetype="<archetype>" version="1">

  <sequence name="<name_1>" id="1" loop="true">
    <!-- TODO: Define action steps -->
    <wait duration="1.0" />
  </sequence>

  <sequence name="<name_2>" id="2">
    <!-- TODO: Define action steps -->
    <wait duration="1.0" />
  </sequence>

</actions>
```

Fill in sensible default actions for each sequence based on the archetype and sequence type. Use the existing `T:/OdysseyEngine/demo/actions/enemy_pack_hunter.actions.xml` as a reference for style and structure.

### 4. Link trigger IDs to the Nadir shader
In the corresponding `.nadir` shader, the `action_request` output field triggers sequences:
```glsl
// In the .nadir shader, trigger a sequence by writing its ID:
outputs[idx].action_request = 1u; // triggers sequence with id="1"
```

### 5. Report
Print:
- Path of the created file
- List of sequences with their IDs
- Reminder to update the corresponding `.nadir` shader to write `action_request` values matching these IDs
- Suggest testing with `/run` to verify the sequences execute correctly

## Reference: Existing Action Files
- `T:/OdysseyEngine/demo/actions/enemy_pack_hunter.actions.xml` (sequences: patrol_route_a, death_sequence, taunt, regroup)
- `T:/OdysseyEngine/demo/actions/multi_arm_gunner.actions.xml`
