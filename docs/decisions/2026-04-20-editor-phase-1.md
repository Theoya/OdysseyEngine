# Decision: Editor Phase 1 — launchable, read-only scene inspector

**Date:** 2026-04-20
**Status:** Implemented (Phase 1 only)
**Driver:** user ask + bless on record: "the user must be able to launch an
`odyssey_editor` executable, open `demo/showcase/showcase.scene.xml`, see
the scene tree, click an entity and see its components in an inspector,
and look at the viewport." Everything else is Phase 2+.

## Summary

Adds a new subsystem directory `src/editor/` and a new executable
`odyssey_editor` that opens a GLFW + Vulkan + ImGui window, loads a
`.scene.xml` via the existing `scene::load_scene_file` pipeline, and
exposes four panels:

1. **SceneTreePanel** — grouped by archetype, click-to-select, live filter.
2. **InspectorPanel** — read-only Transform / Stats / Mesh / Behavior /
   Script / Prefab rows for the selected entity.
3. **ViewportPanel** — Phase 1 placeholder (liminal gradient + moon) with
   the scene name, entity count, and current Mode overlaid.
4. **LogPanel** — spdlog sink ring buffer (512 entries), colored by level.

A mode toolbar (Edit / Play / Simulate) lives in the main menu bar. The
enum and its pure semantics (`mode_runs_nadir`, `mode_runs_scripts`,
`mode_runs_physics`) ship now so Phase 2 engine hooks can consume them
directly.

## Architecture notes & trade-offs

### Why the editor has its own Vulkan loop (not embedded engine)

The existing `odyssey::Engine` owns its swapchain and post-process chain
and does not expose a "here is a command buffer, please record your
overlay" hook. Adding ImGui + panels into `Engine::process_frame` would
have required (a) an ImGui render pass after post-process, (b) threading
through a callback for `set_overlay_fn`, and (c) swapchain-recreate
handling for ImGui's cached pipeline — all for something the user asked
us to ship as Phase 1.

Instead, `odyssey::editor::Editor` owns a minimal Vulkan-for-ImGui setup:
instance, device, swapchain, one render pass, per-frame cmd buffers, and
the ImGui Vulkan backend. It shares the engine's scene/entity data types
but NOT its render loop.

Phase 2 plan: factor `Engine` so the scene render target can be bound via
`ImGui_ImplVulkan_AddTexture` into the ViewportPanel — then the placeholder
goes away and we see the real scene. The Phase 1 code anticipates that:
the viewport panel already reserves the window content-region for an
image, and the engine's `PostProcessor::offscreen_view_` is the exact
`VkImageView` we will bind.

### Why ImGui docking is NOT used in Phase 1

vcpkg ships base ImGui (docking not compiled in — verified in
`build/vcpkg_installed/x64-windows/include/imgui.h`). Rather than vendor
the docking branch and bump our dependency surface, Phase 1 uses the
default multi-window mode. The user drags each panel where they want it;
ImGui remembers positions via `imgui.ini` in the working directory.

Phase 2 may swap to `imgui[docking-experimental]` or a vendored copy if
the council approves (new-dependency trigger per CLAUDE.md).

### Pure vs impure boundaries

Panels derive from `Panel`. `Panel::draw()` is the I/O boundary — ImGui
calls live there. Pure helpers (`entity_display_label`,
`is_static_archetype`, `mode_runs_*`, `LogRingBuffer::push/at`) live in
free functions and are covered by `tests/unit/editor/test_editor_helpers.cpp`.

### Mode semantics

| Mode     | Nadir | Scripts | Physics |
|----------|-------|---------|---------|
| Edit     |  no   |   no    |   no    |
| Play     |  yes  |   yes   |   yes   |
| Simulate |  yes  |   no    |   yes   |

Phase 1 wires this into `editor::Mode` and a pure predicate set. Phase 2
wires these predicates into `Engine::process_frame` via
`Engine::set_mode()` (stub exists nowhere yet — Phase 2 will add it).

## What Phase 1 does NOT do

- No edits on the inspector fields. Every row is read-only. A
  `// Phase 1: read-only` banner at the bottom makes this explicit.
- No live render of the engine's offscreen target — viewport is a
  placeholder. Re-stated above.
- No swapchain recreation on resize. We log and skip; Phase 2 fixes.
- No serialization. Nothing gets written back to `showcase.scene.xml`.
- No play-mode runtime. The Play button flips the enum; nothing downstream
  reads it yet.

## Files added

- `src/editor/mode_enum.h`
- `src/editor/panel.h`
- `src/editor/editor.h` / `editor.cpp`
- `src/editor/scene_tree_panel.h` / `.cpp`
- `src/editor/inspector_panel.h` / `.cpp`
- `src/editor/viewport_panel.h` / `.cpp`
- `src/editor/log_panel.h` / `.cpp`
- `src/app/odyssey_editor_main.cpp`
- `tests/unit/editor/test_editor_helpers.cpp`
- `CMakeLists.txt` — new `odyssey_editor` target + `imgui::imgui` linked
  into the engine library.

## Council note

Phase 1 was blessed out-of-band by the user ("I've already escalated the
broader plan and it's blessed — don't re-invoke the council for this
implementation"). This record closes the loop on the CLAUDE.md mandate
that "new subsystem directory under `src/`" is a council trigger.
