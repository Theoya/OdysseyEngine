#pragma once

// ---------------------------------------------------------------------------
// viewport_panel.h
// Viewport panel.
//
// Phase 1: renders a static placeholder (the scene title, entity counts,
// and a 'liminal mood' gradient drawn via ImGui::GetWindowDrawList). The
// existing engine's offscreen render target is NOT yet bound as a live
// ImGui texture — that ships in Phase 2 when the editor embeds the engine
// render loop (the current Engine owns its swapchain directly, so hooking
// the offscreen view into ImGui::Image requires a refactor we scoped out
// of Phase 1).
//
// Phase 2: replace the placeholder body with ImGui::Image of
// ImGui_ImplVulkan_AddTexture(offscreen_sampler, offscreen_view,
// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL).
// ---------------------------------------------------------------------------

#include "editor/panel.h"

#include <string>

namespace odyssey::editor {

class ViewportPanel : public Panel {
public:
    ViewportPanel();

    const std::string& name() const override { return name_; }
    void draw(EditorState& state) override;

private:
    std::string name_ = "Viewport";
};

} // namespace odyssey::editor
