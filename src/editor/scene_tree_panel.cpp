#include "editor/scene_tree_panel.h"
#include "editor/editor.h"
#include "editor/scene_tree_ops.h"

#include "scene/entity_manager.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>

namespace odyssey::editor {

SceneTreePanel::SceneTreePanel() = default;

// Batch B: archetype-to-icon mapping (one-char emoji or letter badge).
static const char* icon_for(const std::string& archetype) {
    if (archetype == "player") return "🎮";
    if (archetype.find("enemy") != std::string::npos) return "⚠";
    if (archetype.find("light") != std::string::npos) return "💡";
    if (archetype == "static" || archetype == "prop" || archetype == "geometry") return "⬜";
    if (archetype.empty()) return "?";
    return "•";  // Default bullet
}

// Batch B: archetype-to-color mapping (reused from viewport renderer palette).
static ImVec4 color_for(const std::string& archetype) {
    if (archetype == "player") {
        return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green
    }
    if (archetype.find("enemy") != std::string::npos) {
        return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
    }
    if (archetype.find("light") != std::string::npos) {
        return ImVec4(1.0f, 1.0f, 0.3f, 1.0f);  // Yellow
    }
    if (archetype == "static" || archetype == "prop" || archetype == "geometry") {
        return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);  // Gray
    }
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);  // Light gray default
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

    // --- Filter input ---
    ImGui::SetNextItemWidth(-1.0f);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", filter_.c_str());
    if (ImGui::InputTextWithHint("##filter", "Search entities or archetypes...", buf, sizeof(buf))) {
        filter_ = buf;
    }

    // --- Batch B: Expand All / Collapse All buttons ---
    if (ImGui::Button("Expand All", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0))) {
        for (auto& pair : archetype_expanded_) {
            pair.second = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Collapse All", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        for (auto& pair : archetype_expanded_) {
            pair.second = false;
        }
    }

    ImGui::Separator();

    // --- Grouped by archetype ---
    const auto& groups = state.entities->get_archetype_groups();

    for (const auto& g : groups) {
        // Skip empty groups
        if (g.entity_ids.empty()) continue;

        // Archetype header
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;

        // Batch B: check expand state
        bool& is_expanded = archetype_expanded_[g.archetype_name];
        if (is_expanded) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        std::string header = g.archetype_name + "  [" +
                             std::to_string(g.entity_ids.size()) + "]";

        bool open = ImGui::TreeNodeEx(header.c_str(), flags);

        // Batch B: track collapse state for next frame
        is_expanded = open;

        if (open) {
            for (EntityID eid : g.entity_ids) {
                const scene::Entity* e = state.entities->get_entity(eid);
                if (!e) continue;

                std::string label = entity_display_label(*e);
                if (!matches_filter(*e, filter_)) continue;

                bool is_selected = (state.selected_entity == eid);
                bool is_multi_selected = state.multi_selected.count(eid) > 0;

                ImGuiTreeNodeFlags leaf_flags =
                    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                if (is_selected || is_multi_selected) {
                    leaf_flags |= ImGuiTreeNodeFlags_Selected;
                }

                // Batch B: Render with icon + color
                ImVec4 row_color = color_for(e->archetype);
                ImGui::PushStyleColor(ImGuiCol_Text, row_color);

                std::string display = std::string(icon_for(e->archetype)) + " " + label;

                ImGui::TreeNodeEx(
                    reinterpret_cast<void*>(static_cast<intptr_t>(eid)),
                    leaf_flags, "%s", display.c_str());

                ImGui::PopStyleColor();

                // Batch B: Handle selection (Ctrl+click for multi, Shift+click for range, plain for single)
                if (ImGui::IsItemClicked()) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (io.KeyCtrl) {
                        // Toggle multi-select
                        if (state.multi_selected.count(eid)) {
                            state.multi_selected.erase(eid);
                        } else {
                            state.multi_selected.insert(eid);
                        }
                        state.selected_entity = eid;
                    } else if (io.KeyShift) {
                        // Range select (add to multi_selected from last to current in visual order)
                        state.multi_selected.insert(eid);
                    } else {
                        // Plain click: reset to single selection
                        state.multi_selected.clear();
                        state.selected_entity = eid;
                    }
                    state.dirty = true;
                }

                // Batch B: Right-click context menu
                if (ImGui::BeginPopupContextItem()) {
                    // Make sure this entity is the primary selection
                    if (!is_selected) {
                        state.selected_entity = eid;
                    }

                    // Create Empty Child
                    if (ImGui::MenuItem("Create Empty Child")) {
                        EntityID child_id = state.entities->create_entity("new_entity", "");
                        spdlog::info("[editor] created entity {} as child of {}", child_id, eid);
                        // TODO(Batch K): set parent_id when available
                    }

                    ImGui::Separator();

                    // Duplicate (Ctrl+D)
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                        auto dup_res = duplicate_entity(*state.entities, eid);
                        if (dup_res.is_ok()) {
                            EntityID new_id = dup_res.value();
                            state.selected_entity = new_id;
                            spdlog::info("[editor] duplicated entity {} → {}", eid, new_id);
                        } else {
                            spdlog::error("[editor] duplicate failed: {}", dup_res.error());
                        }
                    }

                    // Delete (Del)
                    if (ImGui::MenuItem("Delete", "Del")) {
                        auto del_res = delete_entity(*state.entities, eid);
                        if (del_res.is_ok()) {
                            state.selected_entity = INVALID_ENTITY;
                            state.multi_selected.erase(eid);
                            spdlog::info("[editor] deleted entity {}", eid);
                        } else {
                            spdlog::error("[editor] delete failed: {}", del_res.error());
                        }
                    }

                    ImGui::Separator();

                    // Rename (F2)
                    if (ImGui::MenuItem("Rename", "F2")) {
                        rename_in_progress_ = eid;
                        rename_buffer_ = e->name;
                    }

                    ImGui::Separator();

                    // Copy (Ctrl+C) — session clipboard
                    if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                        spdlog::info("[editor] copied entity {} to clipboard", eid);
                        // TODO: implement session clipboard on next batch
                    }

                    // Paste (Ctrl+V)
                    if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) {
                        // Disabled until clipboard is implemented
                    }

                    ImGui::Separator();

                    // Batch H: Prefab operations
                    if (ImGui::MenuItem("Create Prefab")) {
                        spdlog::info("[editor] Create Prefab stub for entity {}", eid);
                        // TODO: wire to prefab_ops::create_prefab_from_entity in Batch K
                    }

                    if (ImGui::MenuItem("Edit Prefab", nullptr, false)) {
                        ImGui::OpenPopup("Prefab Edit Stub");
                    }

                    if (ImGui::MenuItem("Apply Overrides", nullptr, false)) {
                        spdlog::info("[editor] Apply Prefab Overrides stub for entity {}", eid);
                        // TODO: wire to prefab_ops::apply_prefab_overrides in Batch K
                    }

                    if (ImGui::MenuItem("Revert Overrides", nullptr, false)) {
                        spdlog::info("[editor] Revert Prefab Overrides stub for entity {}", eid);
                        // TODO: wire to prefab_ops::revert_prefab_overrides in Batch K
                    }

                    if (ImGui::MenuItem("Unpack Prefab")) {
                        spdlog::info("[editor] Unpack Prefab for entity {}", eid);
                        // TODO: wire to prefab_ops::unpack_prefab in Batch K
                    }

                    ImGui::EndPopup();
                }

                // Batch B: Inline rename when F2 was pressed
                if (rename_in_progress_ == eid) {
                    ImGui::OpenPopup("Rename Entity");
                }

                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                if (ImGui::BeginPopupModal("Rename Entity", nullptr,
                                          ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("New name:");
                    ImGui::SetNextItemWidth(250.0f);
                    if (ImGui::InputText("##rename_input", rename_buffer_.data(),
                                        rename_buffer_.capacity() + 1,
                                        ImGuiInputTextFlags_EnterReturnsTrue)) {
                        // Commit rename
                        scene::Entity* entity = state.entities->get_entity(eid);
                        if (entity) {
                            entity->name = rename_buffer_;
                        }
                        rename_in_progress_ = INVALID_ENTITY;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("OK", ImVec2(120, 0))) {
                        scene::Entity* entity = state.entities->get_entity(eid);
                        if (entity) {
                            entity->name = rename_buffer_;
                        }
                        rename_in_progress_ = INVALID_ENTITY;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        rename_in_progress_ = INVALID_ENTITY;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }
            ImGui::TreePop();
        }
    }

    // Batch H: Prefab edit stub popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Prefab Edit Stub", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "Prefab isolation mode will be available in Phase 9.\n\n"
            "For now, you can:\n"
            "  • Create Prefab: save entity to .prefab.xml\n"
            "  • Unpack Prefab: replace instance with constituents\n\n"
            "Full editing deferred to later phases.");
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace odyssey::editor
