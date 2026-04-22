#include "editor/scene_tree_panel.h"
#include "editor/editor.h"

#include "scene/entity_manager.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

namespace odyssey::editor {

SceneTreePanel::SceneTreePanel() = default;

// Pure helper: case-insensitive substring check. Kept here because only
// this panel uses it.
static bool contains_ci(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    if (needle.size() > hay.size()) return false;
    auto it = std::search(
        hay.begin(), hay.end(),
        needle.begin(), needle.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });
    return it != hay.end();
}

void SceneTreePanel::draw(EditorState& state) {
    if (!visible_) return;

    if (!ImGui::Begin(name_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    // --- Header ---
    if (state.entities) {
        if (state.scene_path.empty()) {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "No scene loaded");
        } else {
            ImGui::TextUnformatted(("Scene: " + state.scene_path.string()).c_str());
            if (state.entities && state.entities->entity_count() == 0) {
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                                   "0 entities — is the path correct?");
            }
        }
        ImGui::Separator();
        ImGui::Text("Entities: %u", state.entities->entity_count());
    } else {
        ImGui::TextDisabled("No scene loaded.");
        ImGui::End();
        return;
    }

    // --- Filter ---
    ImGui::SetNextItemWidth(-1.0f);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", filter_.c_str());
    if (ImGui::InputTextWithHint("##filter", "Search entities...", buf, sizeof(buf))) {
        filter_ = buf;
    }

    ImGui::Separator();

    // --- Grouped by archetype ---
    const auto& groups = state.entities->get_archetype_groups();

    for (const auto& g : groups) {
        // Skip empty groups
        if (g.entity_ids.empty()) continue;

        // Archetype header
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
                                   ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;

        std::string header = g.archetype_name + "  [" +
                             std::to_string(g.entity_ids.size()) + "]";

        if (is_static_archetype(g.archetype_name)) {
            // Visual cue: static geometry in grey
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 160, 170, 255));
        }
        bool open = ImGui::TreeNodeEx(header.c_str(), flags);
        if (is_static_archetype(g.archetype_name)) {
            ImGui::PopStyleColor();
        }

        if (open) {
            for (EntityID eid : g.entity_ids) {
                const scene::Entity* e = state.entities->get_entity(eid);
                if (!e) continue;

                std::string label = entity_display_label(*e);
                if (!contains_ci(label, filter_)) continue;

                bool is_selected = (state.selected_entity == eid);
                ImGuiTreeNodeFlags leaf_flags =
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (is_selected) leaf_flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(
                    reinterpret_cast<void*>(static_cast<intptr_t>(eid)),
                    leaf_flags, "%s", label.c_str());
                if (ImGui::IsItemClicked()) {
                    state.selected_entity = eid;
                    state.dirty = true;
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

} // namespace odyssey::editor
