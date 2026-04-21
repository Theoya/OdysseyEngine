#include "editor/viewport_panel.h"
#include "editor/editor.h"

#include "scene/entity_manager.h"

#include <imgui.h>

#include <cmath>

namespace odyssey::editor {

ViewportPanel::ViewportPanel() = default;

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
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // --- Phase 1 placeholder: a liminal vertical gradient evoking the
    //     showcase lighting profile (moon over sun, aura over albedo). ---
    ImU32 top    = IM_COL32( 18,  22,  40, 255);   // cold indigo
    ImU32 mid    = IM_COL32( 60,  55,  95, 255);   // dusk violet
    ImU32 bottom = IM_COL32(120,  80, 100, 255);   // rose-grey horizon
    ImVec2 pmid  = ImVec2(p0.x, (p0.y + p1.y) * 0.5f);
    dl->AddRectFilledMultiColor(p0, ImVec2(p1.x, pmid.y), top, top, mid, mid);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, pmid.y), p1, mid, mid, bottom, bottom);

    // Horizon line
    dl->AddLine(ImVec2(p0.x, pmid.y), ImVec2(p1.x, pmid.y),
                IM_COL32(220, 200, 220, 40), 1.0f);

    // --- Moon ---
    float cx = p0.x + avail.x * 0.68f;
    float cy = p0.y + avail.y * 0.28f;
    dl->AddCircleFilled(ImVec2(cx, cy), 28.0f, IM_COL32(230, 230, 245, 255), 32);
    // Moon aura
    for (int i = 1; i <= 6; ++i) {
        float r = 28.0f + i * 4.0f;
        int alpha = 22 - i * 3;
        if (alpha < 0) alpha = 0;
        dl->AddCircle(ImVec2(cx, cy), r,
                      IM_COL32(230, 230, 245, alpha), 32, 1.5f);
    }

    // --- Overlay text ---
    dl->AddRectFilled(p0,
                      ImVec2(p0.x + 260.0f, p0.y + 60.0f),
                      IM_COL32(0, 0, 0, 120));
    std::string title = "VIEWPORT (Phase 1 placeholder)";
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 8.0f),
                IM_COL32(220, 220, 255, 255), title.c_str());

    std::string sub = "scene: " + state.scene_path.filename().string();
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 28.0f),
                IM_COL32(180, 180, 210, 255), sub.c_str());

    std::string stat = "entities: ";
    stat += state.entities ? std::to_string(state.entities->entity_count()) : "—";
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 44.0f),
                IM_COL32(180, 180, 210, 255), stat.c_str());

    // Mode label, bottom-right
    std::string modestr = std::string("[") + std::string(mode_label(state.mode)) + "]";
    ImVec2 ts = ImGui::CalcTextSize(modestr.c_str());
    dl->AddText(ImVec2(p1.x - ts.x - 10.0f, p1.y - ts.y - 8.0f),
                IM_COL32(220, 220, 255, 220), modestr.c_str());

    // Reserve the space so the window sizes correctly
    ImGui::Dummy(avail);

    ImGui::End();
}

} // namespace odyssey::editor
