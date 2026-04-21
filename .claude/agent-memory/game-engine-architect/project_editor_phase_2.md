---
name: Editor Phase 2 shape
description: Phase 2 landed the live viewport, preserve-unknowns serializer, and mode gating; pins architectural decisions that future phases must respect.
type: project
---

Phase 2 closed the three items Phase 1 deferred:

1. **Live viewport** — editor now has its OWN offscreen scene renderer (`src/editor/scene_viewport_renderer.{h,cpp}`) that draws one colored cube per entity. Bound into ImGui via `ImGui_ImplVulkan_AddTexture(offscreen_sampler, offscreen_view, SHADER_READ_ONLY_OPTIMAL)`. Editor still does NOT embed `odyssey::Engine`; Phase 3+ can do that refactor.

2. **Preserve-unknowns serializer** — `src/scene/scene_serializer.{h,cpp}`. `SceneData::preserved_source` stores raw file bytes; unmutated serialize = echo. `SceneData::mutated` flips to reconstruction path. Round-trip is byte-identical against both showcase.scene.xml and shooter_arena.scene.xml.

3. **Mode gating** — `Mode` enum moved to `src/core/mode.h`. `Engine::set_mode()` gates `record_dispatches` + game-tick delta_time + skips game tick entirely in Edit. `GameContext::mode` is forwarded so games gate their own script/physics.

**Why (motivation):** Phase 1 was a shell; Phase 2 is the first release where the editor is genuinely useful for inspection (live view) and round-trip authoring (serializer). The mode gating unblocks the `/mode-switch` acceptance criterion in the showcase report.

**How to apply:**
- `Mode` enum lives in `odyssey::` (core/mode.h). The `odyssey::editor::Mode` alias in `editor/mode_enum.h` is a compatibility shim — new code should use `odyssey::Mode` directly.
- If you edit `SceneData` through the API, set `mutated=true` so the serializer switches to reconstruction. Tests assert both paths.
- The editor's scene renderer is intentionally minimal (colored cubes). Do NOT grow it into a re-implementation of `odyssey::vulkan::Renderer` — if that's needed, the correct move is Phase 3's "embed the engine" refactor.
- Barrier contract: `SceneViewportRenderer`'s scene render pass finalLayout is `SHADER_READ_ONLY_OPTIMAL` and it carries an explicit `COLOR_ATTACHMENT_OUTPUT_BIT → FRAGMENT_SHADER_BIT` subpass dependency so no manual `vkCmdPipelineBarrier` is needed. A compile-time `static_assert` in `test_viewport_helpers.cpp` pins this.
- `PostProcessor::offscreen_view()` / `offscreen_sampler()` accessors exist for when a hosted-engine editor path does land — the /barrier-audit comment on those accessors documents the same sampler contract.

**Phase 4 NOT done here (explicit deferrals):** Inspector editable fields, asset browser, hot reload, multi-scene, prefab editor. When Inspector becomes writable, the contract is: on every mutation, flip `SceneData::mutated = true` so `serialize_scene_to_string` takes the reconstruction path.
