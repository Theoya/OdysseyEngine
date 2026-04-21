---
name: Nadir Weights UBO convention
description: Hot-tunable per-archetype weights live in a std140 UBO at set=0 binding=7; driven by /tune-weights without recompile
type: project
---

Every scoring-heavy `.nadir` should declare a Weights UBO so tuning happens
without recompile.

**Why:** the editor-mutable-beaver plan's ai-engineer condition on the inaugural
council vote specifically required hot-tunable weight UBO and deterministic
replay as editor-foundation bars. Recompile-per-tuning-pass breaks feel
iteration speed.

**How to apply:** any literal that appears in a score formula — thresholds,
ranges, cooldowns, hysteresis bonuses, flocking weights — goes into the UBO.
Layout:

```glsl
layout(std140, set = 0, binding = 7) uniform Weights {
    float aggression;
    float cohesion;
    float flee_hp;
    // ... pad to 16-float vec4 rows
    float _pad[N];
} W;
```

Structural changes (new state, new consideration, new output field) still
require recompile via `/nadir-compile`. UBO-slot additions require a one-time
recompile then all subsequent tuning is live.

The `BehaviorCompiler` preamble (`src/nadir/behavior_compiler.cpp` around the
SSBO binding block) is the place to add a canonical Weights binding so every
archetype can opt in without per-file preamble noise. As of writing, this
addition has not been made in-repo — it is part of the Phase-1 editor
foundation work.
