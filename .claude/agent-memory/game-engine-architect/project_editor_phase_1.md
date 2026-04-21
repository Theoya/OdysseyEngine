---
name: Editor Phase 1 shape
description: What the odyssey_editor executable does today and what it explicitly defers to Phase 2, including ImGui docking and viewport-image sourcing decisions.
type: project
---

`odyssey_editor` is the Phase-1 scene-inspector executable. It deliberately does NOT embed the engine's render loop — it runs its own minimal GLFW + Vulkan + ImGui main loop that shares only the scene/entity types with the engine.

**Why:** The existing `Engine::process_frame` owns the swapchain and post-process chain without an overlay hook. Adding ImGui into the engine loop would require a new render pass after post-process + a callback API + swapchain-recreation handling for ImGui's cached pipeline. The user approved shipping Phase 1 as a standalone executable, with Phase 2 doing the engine integration properly.

**How to apply:**
- The ViewportPanel is a PLACEHOLDER (liminal gradient + moon). Do NOT claim it shows the live render target until Phase 2 wires `ImGui_ImplVulkan_AddTexture(offscreen_sampler, offscreen_view, SHADER_READ_ONLY_OPTIMAL)`.
- Docking is NOT enabled — vcpkg ships base ImGui only (confirmed: no `IMGUI_HAS_DOCK` in `build/vcpkg_installed/x64-windows/include/imgui.h`). Phase 1 uses multi-window mode; swapping to docking requires a new-dependency council vote.
- Mode toolbar wires `editor::Mode` enum, but nothing downstream pauses Nadir/scripts yet — Phase 2 adds `Engine::set_mode()`.
- Inspector is read-only by design. Editing requires the four-step ritual (extend EntityComponents, extend XSD, write the Inspector sub-editor, extend the serializer) per `schema-add`.
- Editor swapchain recreation on window resize is a no-op that logs a warning. Phase 2 fixes.
