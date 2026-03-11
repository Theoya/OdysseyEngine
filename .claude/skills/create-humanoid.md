---
description: Create a complete humanoid character with skeleton, animations, and optionally a behavior shader
user-invocable: true
---

# /create-humanoid

Usage: `/create-humanoid <name> [--height <m>] [--with-gun] [--enemy]`

## Instructions

When the user invokes this skill:

1. Parse arguments:
   - `<name>`: required, character name
   - `--height`: optional, defaults to 1.8m
   - `--with-gun`: optional flag, adds weapon attachment points
   - `--enemy`: optional flag, creates a behavior shader

2. Determine output directory: look for existing `demo/` directory structure, create `demo/<name>/assets/` if needed.

3. Create skeleton:
   - Use the `/create-skeleton` approach: generate humanoid 19-bone skeleton
   - Scale to --height
   - If --with-gun: include right_hand_grip, left_hand_grip, muzzle attachments
   - Write to `<name>.skeleton.xml`

4. Create animations:
   - Generate walk cycle: `walk_cycle.anim.xml` (0.8s loop, 4-phase)
   - Generate idle: `idle.anim.xml` (2.0s breathing bob)

5. If --enemy:
   - Create a behavior shader `<name>.nadir` adapted from enemy_pack_hunter template:
     - State machine: IDLE → CHASE → ATTACK → RETREAT
     - Chase when player within 40 units
     - Attack when within 10 units
     - Retreat when health < 25%
     - Output move_vector toward/away from player
     - Output attack_target when in attack state

6. Create a prefab XML:
   ```xml
   <?xml version="1.0" encoding="UTF-8"?>
   <prefab name="NAME" archetype="NAME">
     <skeleton path="NAME.skeleton.xml"/>
     <animation_set>
       <clip name="walk" path="walk_cycle.anim.xml" default="true"/>
       <clip name="idle" path="idle.anim.xml"/>
     </animation_set>
   </prefab>
   ```

7. Report all created files and next steps:
   - How to add the character to a scene
   - How to test the animations
   - How to customize bone proportions
