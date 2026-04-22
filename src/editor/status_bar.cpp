#include "editor/status_bar.h"
#include "editor/editor.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace odyssey::editor {

float compute_fps_ema(float prev, float new_val, float alpha) {
    // Clamp alpha to [0, 1].
    alpha = std::max(0.0f, std::min(1.0f, alpha));
    return alpha * new_val + (1.0f - alpha) * prev;
}

void draw_status_bar(EditorState& state, float fps, float dt_ms) {
    // Get viewport size for bottom pinning.
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetMainViewport()->WorkPos.x,
               ImGui::GetMainViewport()->WorkPos.y +
                   ImGui::GetMainViewport()->WorkSize.y -
                   ImGui::GetFrameHeight() - 4),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(ImGui::GetMainViewport()->WorkSize.x,
               ImGui::GetFrameHeight() + 8),
        ImGuiCond_Always);

    if (!ImGui::Begin("##StatusBar",
                      nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    // Layout: left-aligned items, then separator, then right-aligned.
    ImGui::BeginGroup();

    // Scene path (left).
    if (state.scene_path.empty()) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Scene: No scene");
    } else {
        ImGui::Text("Scene: %s", state.scene_path.filename().string().c_str());
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Entity count.
    if (state.entities) {
        ImGui::Text("%d entities", state.entities->entity_count());
    } else {
        ImGui::Text("0 entities");
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Selection.
    if (state.selected_entity != INVALID_ENTITY && state.entities) {
        auto ent = state.entities->get_entity(state.selected_entity);
        if (ent) {
            ImGui::Text("Sel: %s", entity_display_label(*ent).c_str());
        } else {
            ImGui::Text("Sel: <invalid>");
        }
    } else {
        ImGui::Text("Sel: none");
    }

    ImGui::EndGroup();

    // Right-aligned items.
    float available_width = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(available_width - 200.0f);

    ImGui::Separator();
    ImGui::SameLine();

    // FPS (right).
    ImGui::Text("%.1f FPS", fps);

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Mode pill (colored).
    ImVec4 mode_color;
    const char* mode_text = "?";
    if (state.mode == Mode::Edit) {
        mode_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        mode_text = "EDIT";
    } else if (state.mode == Mode::Play) {
        mode_color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
        mode_text = "PLAY";
    } else {
        mode_color = ImVec4(0.2f, 0.2f, 0.8f, 1.0f);
        mode_text = "SIM";
    }

    ImGui::PushStyleColor(ImGuiCol_Button, mode_color);
    ImGui::SmallButton(mode_text);
    ImGui::PopStyleColor();

    ImGui::End();
}

}  // namespace odyssey::editor
