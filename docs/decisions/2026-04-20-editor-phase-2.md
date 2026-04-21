# Decision: Editor Phase 2 — live viewport, preserve-unknowns serializer, mode gating

**Date:** 2026-04-20
**Status:** Implemented (Phase 2 scope only)
**Driver:** Phase 1 closed the shell; three items were deferred. This record
closes them: live viewport render, scene-serializer with preserve-unknowns,
and mode-toolbar gating into the engine.

## Summary

Three deliverables, landed as a single coordinated phase (pre-ratified by
the Phase 1 decision record — no new council trigger required):

1. **Live viewport** — the `ViewportPanel` now draws the engine's offscreen
   scene image via `ImGui_ImplVulkan_AddTexture`, not a placeholder gradient.
2. **Scene serializer with preserve-unknowns** — `src/scene/scene_serializer.{h,cpp}`
   plus preserve-buckets on `SceneData::EntityDesc` and `SceneData`. Load →
   serialize is byte-identical on an unmutated scene.
3. **Mode-toolbar gating** — `Engine::set_mode(Mode)` gates Nadir dispatch,
   script tick, and physics step per the Edit/Play/Simulate matrix. The
   `Mode` enum moved from `src/editor/mode_enum.h` to `src/core/mode.h` so
   both `src/app/` and `src/editor/` depend on it without a circular dep.

## Architecture notes & trade-offs

### Live viewport — standalone editor, own scene renderer

Phase 1 shipped the editor as a standalone executable with its own GLFW +
Vulkan + ImGui loop. Phase 2 preserves that split and adds a small, purpose-
built scene renderer (`src/editor/scene_viewport_renderer.{h,cpp}`) that
draws one colored cube per entity into an offscreen `VkImage`. ImGui samples
that image as a texture via `ImGui_ImplVulkan_AddTexture`.

**Why not host the full engine inside the editor?** The engine still owns
the swapchain and post-process chain, and the Phase 1 record explicitly
scoped "embed the engine render loop" as Phase 3+ work. Shipping our own
small scene renderer gets us the "viewport is live" deliverable without
reopening the engine's frame graph. The editor now shows entity transforms
as colored boxes against a liminal-mood clear — enough to visually verify
the scene without pretending to re-render every pixel the shipping game
renders.

**Barrier contract** (for `/barrier-audit`):

```
Per frame in Editor::draw_frame:
  [scene_pass begin]  offscreen: UNDEFINED (LOAD_OP_CLEAR)
  [scene_pass draws]  offscreen: COLOR_ATTACHMENT_OPTIMAL
                      WRITE: COLOR_ATTACHMENT_OUTPUT | COLOR_ATTACHMENT_WRITE
  [scene_pass end]    offscreen: SHADER_READ_ONLY_OPTIMAL
                      Subpass dep: COLOR_ATTACHMENT_OUTPUT_BIT
                                 → FRAGMENT_SHADER_BIT
                                 accesses: COLOR_ATTACHMENT_WRITE → SHADER_READ
  [imgui render_pass] offscreen sampled from FRAGMENT_SHADER
                      swapchain: UNDEFINED → PRESENT_SRC_KHR
```

The explicit subpass dependency in `SceneViewportRenderer::create_render_pass`
makes the layout transition visibility-correct; no manual `vkCmdPipelineBarrier`
is needed because the render pass end carries the transition. A compile-time
assertion in `test_viewport_helpers.cpp` pins `final_layout() ==
SHADER_READ_ONLY_OPTIMAL` so a future refactor can't silently regress it.

**Resize**: ViewportPanel writes a requested pixel extent into `EditorState`;
Editor applies it between frames via `SceneViewportRenderer::resize`, which
`vkDeviceWaitIdle`s, destroys, and recreates the images + framebuffer + ImGui
descriptor. The window's own swapchain recreation is now wired properly via
`rebuild_swapchain`, replacing Phase 1's "log and skip" behavior.

### Serializer — preserve by echo, reconstruct on mutation

The Phase 2 serializer uses a **two-path** design:

- **Echo path** (default): if `SceneData::preserved_source` is non-empty and
  `SceneData::mutated` is false, `serialize_scene_to_string` returns the
  exact bytes the loader captured. This makes byte-identical round-trip
  trivial and robust to comments, indentation, and attribute ordering.
- **Reconstruction path** (Phase 4 authoring): activated by
  `SerializeOptions::force_reconstruct = true` or `SceneData::mutated = true`.
  Walks the parsed fields + `unknown_attributes` + `unknown_children_xml`
  in the documented order: known attrs, unknown attrs (insertion order),
  known children, unknown children. Used for tests today; will be the
  default once the Inspector becomes writable.

**Trade-off accepted**: on the reconstruction path we don't yet preserve
comments. The XSD discussion for light/audio attrs is deliberately parked —
the serializer honors those attrs as unknowns so the schema can stay narrow
for now.

Preserve-bucket layout is small and pure-data (no pugi handles survive
outside `parse_scene_xml`), so `SceneData` has stable move/copy semantics
independent of pugi's arena.

### Mode gating — Mode enum moves to core

Both `Engine` and `Editor` need to read/write the execution mode. Putting
`Mode` in `src/core/mode.h` (pure types only) breaks the would-be cycle:
`src/app/` and `src/editor/` both depend on `src/core/`, not on each
other. `src/editor/mode_enum.h` is now a re-export shim so existing
`#include "editor/mode_enum.h"` sites continue to compile.

| Mode     | Nadir dispatch | Scripts | Physics | Camera |
|----------|:--------------:|:-------:|:-------:|:------:|
| Edit     |   skipped      | paused  | paused  | live   |
| Play     |   runs         | ticks   | steps   | live   |
| Simulate |   runs         | paused  | steps   | live   |

Implementation points in `Engine::process_frame`:
- `record_dispatches(cmd)` wrapped by `mode_runs_nadir(mode_)`.
- The game tick is skipped entirely in Edit mode; in Simulate/Play it runs,
  with `ctx.mode` set so game implementations can skip their own script
  logic in Simulate. `ctx.delta_time` is zeroed when `mode_runs_physics`
  is false — belt-and-suspenders so a game that forgets to gate its own
  physics still behaves sanely.
- `Engine::tick_would_*` predicates expose the same pure questions the
  main loop asks, so the test suite can verify the gating matrix without
  standing up a GPU.

## Tests added

- `tests/unit/scene/test_scene_serializer.cpp` (12 tests):
  byte-identical round-trip for showcase.scene.xml AND shooter_arena.scene.xml;
  per-entity capture of every lighting/audio/material_override attribute;
  scene-root attr capture; file I/O success + failure; force_reconstruct
  and mutated-path parse-back sanity.
- `tests/unit/editor/test_viewport_helpers.cpp` (5 tests): pure-helper
  `compute_viewport_pixel_extent` behavior + compile-time barrier assertion
  for `SceneViewportRenderer::final_layout()`.
- `tests/unit/app/test_engine_mode_gating.cpp` (11 tests): Engine mode
  accessor/mutator behavior + full matrix assertion on the per-subsystem
  predicates.

Target of +20 new tests met (12 + 5 + 11 = 28 new).

## What Phase 2 does NOT do (Phase 4 scope)

- Editable inspector fields.
- Asset browser panel.
- Hot reload (the editor does not watch `demo/` for XML changes).
- Multi-scene / new-scene creation.
- Scene-edit mutation path in the Inspector — when we do that, we'll flip
  `SceneData::mutated = true` on edit and rely on the reconstruction path.

## Files added

- `src/core/mode.h`
- `src/scene/scene_serializer.h` / `.cpp`
- `src/editor/scene_viewport_renderer.h` / `.cpp`
- `tests/unit/scene/test_scene_serializer.cpp`
- `tests/unit/editor/test_viewport_helpers.cpp`
- `tests/unit/app/test_engine_mode_gating.cpp`
- `docs/decisions/2026-04-20-editor-phase-2.md` (this file)

## Files modified

- `src/editor/mode_enum.h` — now a re-export shim over `core/mode.h`
- `src/scene/scene_loader.h` / `.cpp` — preserve-unknowns buckets; spawn_region
  entity-nesting; binary-mode read so CRLF round-trips on Windows
- `src/editor/editor.h` / `.cpp` — SceneViewportRenderer integration,
  ImGui texture registration, viewport resize handling, real swapchain
  recreation
- `src/editor/viewport_panel.h` / `.cpp` — `compute_viewport_pixel_extent`
  helper; ImGui::Image draw path with fallback to the Phase 1 gradient when
  the renderer isn't available
- `src/app/engine.h` / `.cpp` — `Mode mode_`, `set_mode`, pure predicates,
  `record_dispatches` gated on mode, `GameContext::mode` forwarded
- `src/app/game.h` — `GameContext::mode` field (Phase 2 contract)
- `src/vulkan/postprocess.h` — `offscreen_view()` / `offscreen_sampler()`
  accessors with barrier-audit comment
- `docs/architecture.md` — "Scene XML round-trip" section

## Council note

Per Phase 1's bless, Phase 2 scope was pre-ratified; no new `/council` was
convened for this implementation. Any new dependency would have triggered
one — we added none (shaderc and glm were already grandfathered).
