---
description: Create an animation clip XML file for skeletal animation
user-invocable: true
---

# /create-animation

Usage: `/create-animation <name> --skeleton <skeleton-file> [--type walk|idle|custom] [--duration <sec>]`

## Instructions

When the user invokes this skill:

1. Parse arguments:
   - `<name>`: required, animation name
   - `--skeleton`: required, path to .skeleton.xml file to read bone names from
   - `--type`: optional, defaults to "custom". Options: "walk" (4-phase walk cycle), "idle" (breathing bob), "custom" (empty template)
   - `--duration`: optional, defaults based on type (walk=0.8, idle=2.0, custom=1.0)

2. Read the skeleton XML to get bone names and rest poses.

3. Generate animation based on type:

   **walk**: 4-phase walk cycle (0.8s loop)
   - Root: vertical bob (±0.02 at 0.0/0.4 vs 0.2/0.6)
   - Spine: slight Y counter-rotation
   - Upper legs: alternating X rotation (±0.2 rad) for forward/back swing
   - Lower legs: knee bend during swing phase
   - Upper arms: opposite swing to legs (±0.15 rad)

   **idle**: breathing bob (2.0s loop)
   - Root: subtle Y oscillation (±0.01)
   - Chest: very slight X rotation breathing motion

   **custom**: one keyframe per bone at rest pose at t=0

4. Write to `<name>.anim.xml`.

5. Report created file, track count, and duration.

## XML Format

```xml
<?xml version="1.0" encoding="UTF-8"?>
<animation name="NAME" duration="D" looping="true|false">
  <track bone="bone_name">
    <key time="0.0" position="X Y Z" rotation="QX QY QZ QW"/>
    <key time="0.4" position="X Y Z" rotation="QX QY QZ QW"/>
  </track>
</animation>
```
