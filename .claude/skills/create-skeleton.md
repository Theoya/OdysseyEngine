---
description: Create a skeleton XML file for skeletal animation
user-invocable: true
---

# /create-skeleton

Usage: `/create-skeleton <name> [--type humanoid|custom] [--height <meters>]`

## Instructions

When the user invokes this skill:

1. Parse the arguments:
   - `<name>`: required, the skeleton name (used for filename)
   - `--type`: optional, defaults to "humanoid". Options: "humanoid" (19-bone), "custom" (minimal template)
   - `--height`: optional, defaults to 1.8 for humanoid. Scales all bone positions proportionally.

2. For `--type humanoid`:
   Generate a complete 19-bone humanoid skeleton XML with this hierarchy:
   - root (hips) → spine → chest → neck → head
   - chest → shoulder_l → upper_arm_l → lower_arm_l → hand_l
   - chest → shoulder_r → upper_arm_r → lower_arm_r → hand_r
   - root → upper_leg_l → lower_leg_l → foot_l
   - root → upper_leg_r → lower_leg_r → foot_r

   Include attachment points: right_hand_grip, left_hand_grip, muzzle (on hand_r).
   Scale all Y positions by (height / 1.8).

3. For `--type custom`:
   Generate a minimal skeleton with root + 2 example bones.

4. Write the file to `<name>.skeleton.xml` in the current demo assets directory (look for the nearest `assets/` dir, or create one).

5. Validate:
   - All parent references resolve to existing bone names
   - No circular parent references
   - Root bone has empty parent

6. Report the created file path and bone count.

## XML Format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<skeleton name="NAME">
  <bone name="root" parent="" position="0 Y 0" rotation="0 0 0 1" length="0.1" radius="0.04"/>
  <bone name="child" parent="root" position="X Y Z" rotation="QX QY QZ QW" length="L" radius="R"/>
  <attachment name="grip" bone="hand_r" position="0 0 0" rotation="0 0 0 1"/>
</skeleton>
```
