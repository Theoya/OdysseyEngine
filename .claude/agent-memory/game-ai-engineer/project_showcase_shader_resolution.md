---
name: Showcase shader resolution
description: How showcase.scene.xml resolves <behavior shader="..."/> names through the single Nadir scan dir
type: project
---

`demo/showcase/showcase.scene.xml` references shaders by bare name
(`player_input.nadir`, `enemy_pack_hunter.nadir`, `enemy_ranged.nadir`,
`multi_arm_gunner.nadir`, `civilian_fleeing.nadir`). These files physically
live in `demo/behaviors/` — shared with the shooter demo.

**Why:** `NadirSystem::load_behaviors()` scans a single directory
(`config_.behavior_dir`, set from `engine.xml` `<nadir behavior_dir="..."/>`)
non-recursively via `find_nadir_files`. There is no per-scene override and
no multi-directory search. Keeping the 5 shaders in `demo/behaviors/` is
the less-duplicative option; copying to `demo/showcase/behaviors/` would
require either a second scan root or symlinks.

**How to apply:** Showcase-only shaders (e.g. `brute.nadir`,
`ranger.nadir`, `scout.nadir` that already exist in
`demo/showcase/behaviors/`) need either (a) a loader change to scan a
second dir, or (b) to live in `demo/behaviors/`. The pipeline test
`NadirShowcaseCompile` scans both dirs for compile coverage, but runtime
still only loads from the single configured `behavior_dir`. If you need
showcase to load them at runtime, update `engine.xml` or extend the
loader — do not expect a scene-local override to "just work".
