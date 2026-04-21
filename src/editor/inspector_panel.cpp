#include "editor/inspector_panel.h"
#include "editor/editor.h"

#include "scene/entity_manager.h"

#include <imgui.h>

namespace odyssey::editor {

InspectorPanel::InspectorPanel() = default;

static void row_label(const char* label, const char* value) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(140.0f);
    ImGui::Text("%s", value);
}

static void row_label(const char* label, const std::string& value) {
    row_label(label, value.empty() ? "<none>" : value.c_str());
}

static void row_vec3(const char* label, const vec3& v) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(140.0f);
    ImGui::Text("%.3f, %.3f, %.3f", v.x, v.y, v.z);
}

static void row_quat(const char* label, const quat& q) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(140.0f);
    ImGui::Text("%.3f, %.3f, %.3f, %.3f", q.x, q.y, q.z, q.w);
}

static void row_float(const char* label, float v) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(140.0f);
    ImGui::Text("%.3f", v);
}

void InspectorPanel::draw(EditorState& state) {
    if (!visible_) return;

    if (!ImGui::Begin(name_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    if (!state.entities || state.selected_entity == INVALID_ENTITY) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::Separator();
        ImGui::TextWrapped(
            "Click an entity in the Scene Tree to view its components. "
            "Editing lands in Phase 2.");
        ImGui::End();
        return;
    }

    const scene::Entity* e = state.entities->get_entity(state.selected_entity);
    if (!e) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                           "Entity %u not found (stale selection).",
                           state.selected_entity);
        ImGui::End();
        return;
    }

    // --- Identity ---
    ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1), "%s", e->name.c_str());
    ImGui::TextDisabled("id=%u  archetype=%s  active=%s",
                        e->id, e->archetype.c_str(),
                        e->active ? "yes" : "no");
    ImGui::Separator();

    // --- Transform ---
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& t = e->components.transform;
        row_vec3("position", t.position);
        row_quat("rotation", t.rotation);
        row_vec3("scale",    t.scale);
    }

    // --- Stats ---
    if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& s = e->components.stats;
        row_float("health",     s.health);
        row_float("max_health", s.max_health);
        row_float("ammo",       s.ammo);
        row_float("stamina",    s.stamina);
        row_float("speed",      s.speed);
    }

    // --- Mesh / Material ---
    if (ImGui::CollapsingHeader("Mesh & Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        row_label("mesh",     e->components.mesh_path);
        row_label("material", e->components.material_path);
    }

    // --- Behavior (Nadir) ---
    if (!e->components.behavior_shader.empty()) {
        if (ImGui::CollapsingHeader("Behavior", ImGuiTreeNodeFlags_DefaultOpen)) {
            row_label("shader", e->components.behavior_shader);
        }
    }

    // --- Script ---
    if (!e->components.script_class.empty()) {
        if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
            row_label("class",  e->components.script_class);
            row_label("config", e->components.script_config);
        }
    }

    // --- Prefab source ---
    if (!e->components.prefab_source.empty()) {
        ImGui::Separator();
        row_label("prefab", e->components.prefab_source);
    }

    ImGui::Separator();
    ImGui::TextDisabled("Phase 1: read-only. Phase 2 enables editing.");

    ImGui::End();
}

} // namespace odyssey::editor
