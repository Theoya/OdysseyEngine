#include "editor/viewport_panel.h"
#include "editor/editor.h"
#include "editor/scene_viewport_renderer.h"

#include "scene/entity_manager.h"

#include <imgui.h>

#include <cmath>

namespace odyssey::editor {

ViewportPanel::ViewportPanel() = default;

// Pure helper: given a content-region size, return the pixel dimensions the
// viewport renderer should use. Clamps to a sane minimum to avoid requesting
// a 0x0 offscreen target during window minimization or layout transitions.
//
// Exposed for unit testing (see tests/unit/editor/test_viewport_helpers.cpp).
namespace {
    constexpr uint32_t kMinViewportDim = 64;
}

ViewportPixelExtent compute_viewport_pixel_extent(float content_w, float content_h) {
    uint32_t w = static_cast<uint32_t>(content_w > 0.0f ? content_w : 0.0f);
    uint32_t h = static_cast<uint32_t>(content_h > 0.0f ? content_h : 0.0f);
    if (w < kMinViewportDim) w = kMinViewportDim;
    if (h < kMinViewportDim) h = kMinViewportDim;
    return {w, h};
}

void ViewportPanel::draw(EditorState& state) {
    if (!visible_) return;

    ImGui::SetNextWindowSize(ImVec2(720, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(name_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 p0    = ImGui::GetCursorScreenPos();
    ImVec2 p1    = ImVec2(p0.x + avail.x, p0.y + avail.y);

    // -------- Phase 2: live viewport image --------
    if (state.viewport_renderer && state.viewport_texture_id && avail.x > 0 && avail.y > 0) {
        // Request an offscreen-target resize if the panel's content region
        // has drifted from the renderer's current extent. Editor reads
        // viewport_requested_* between frames.
        VkExtent2D current = state.viewport_renderer->extent();
        auto want = compute_viewport_pixel_extent(avail.x, avail.y);
        if (want.width != current.width || want.height != current.height) {
            state.viewport_requested_width  = want.width;
            state.viewport_requested_height = want.height;
        }

        // Draw the offscreen image. ImGui samples the image as a texture —
        // the explicit barrier is baked into the scene render pass
        // (finalLayout = SHADER_READ_ONLY_OPTIMAL + subpass dep to
        // FRAGMENT_SHADER_BIT), so no per-frame pipeline barrier is needed
        // here. /barrier-audit format:
        //   [scene_pass WRITE] COLOR_ATTACHMENT_OUTPUT_BIT | COLOR_ATTACHMENT_WRITE_BIT
        //       ↓ subpass dep (explicit in SceneViewportRenderer::create_render_pass)
        //   [imgui SAMPLE]    FRAGMENT_SHADER_BIT | SHADER_READ_BIT
        //       Layout: COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
        ImGui::Image(reinterpret_cast<ImTextureID>(state.viewport_texture_id),
                     avail);

        // Overlay text (same HUD info the placeholder showed).
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0,
                          ImVec2(p0.x + 260.0f, p0.y + 60.0f),
                          IM_COL32(0, 0, 0, 120));
        std::string title = "VIEWPORT (live)";
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 8.0f),
                    IM_COL32(220, 220, 255, 255), title.c_str());
        std::string sub = "scene: " + state.scene_path.filename().string();
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 28.0f),
                    IM_COL32(180, 180, 210, 255), sub.c_str());
        std::string stat = "entities: ";
        stat += state.entities ? std::to_string(state.entities->entity_count()) : "-";
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 44.0f),
                    IM_COL32(180, 180, 210, 255), stat.c_str());
        std::string modestr = std::string("[") + std::string(mode_label(state.mode)) + "]";
        ImVec2 ts = ImGui::CalcTextSize(modestr.c_str());
        dl->AddText(ImVec2(p1.x - ts.x - 10.0f, p1.y - ts.y - 8.0f),
                    IM_COL32(220, 220, 255, 220), modestr.c_str());

        ImGui::End();
        return;
    }

    // -------- Fallback: gradient placeholder (no renderer) --------
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 top    = IM_COL32( 18,  22,  40, 255);
    ImU32 mid    = IM_COL32( 60,  55,  95, 255);
    ImU32 bottom = IM_COL32(120,  80, 100, 255);
    ImVec2 pmid  = ImVec2(p0.x, (p0.y + p1.y) * 0.5f);
    dl->AddRectFilledMultiColor(p0, ImVec2(p1.x, pmid.y), top, top, mid, mid);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, pmid.y), p1, mid, mid, bottom, bottom);
    dl->AddLine(ImVec2(p0.x, pmid.y), ImVec2(p1.x, pmid.y),
                IM_COL32(220, 200, 220, 40), 1.0f);

    float cx = p0.x + avail.x * 0.68f;
    float cy = p0.y + avail.y * 0.28f;
    dl->AddCircleFilled(ImVec2(cx, cy), 28.0f, IM_COL32(230, 230, 245, 255), 32);
    for (int i = 1; i <= 6; ++i) {
        float r = 28.0f + i * 4.0f;
        int alpha = 22 - i * 3;
        if (alpha < 0) alpha = 0;
        dl->AddCircle(ImVec2(cx, cy), r,
                      IM_COL32(230, 230, 245, alpha), 32, 1.5f);
    }

    dl->AddRectFilled(p0,
                      ImVec2(p0.x + 260.0f, p0.y + 60.0f),
                      IM_COL32(0, 0, 0, 120));
    std::string title = "VIEWPORT (fallback placeholder)";
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 8.0f),
                IM_COL32(220, 220, 255, 255), title.c_str());

    std::string sub = "scene: " + state.scene_path.filename().string();
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 28.0f),
                IM_COL32(180, 180, 210, 255), sub.c_str());

    std::string stat = "entities: ";
    stat += state.entities ? std::to_string(state.entities->entity_count()) : "-";
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 44.0f),
                IM_COL32(180, 180, 210, 255), stat.c_str());

    std::string modestr = std::string("[") + std::string(mode_label(state.mode)) + "]";
    ImVec2 ts = ImGui::CalcTextSize(modestr.c_str());
    dl->AddText(ImVec2(p1.x - ts.x - 10.0f, p1.y - ts.y - 8.0f),
                IM_COL32(220, 220, 255, 220), modestr.c_str());

    ImGui::Dummy(avail);
    ImGui::End();
}

} // namespace odyssey::editor
