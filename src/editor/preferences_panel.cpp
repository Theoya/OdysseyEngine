#include "editor/preferences_panel.h"
#include "editor/editor.h"

#include <imgui.h>

#include <spdlog/spdlog.h>

namespace odyssey::editor {

PreferencesPanel::PreferencesPanel(const std::filesystem::path& exe_dir)
    : exe_dir_(exe_dir) {
    // Load existing preferences.
    auto r = load_preferences(exe_dir);
    if (r.is_ok()) {
        prefs_ = r.value();
    } else {
        spdlog::warn("[editor] failed to load preferences: {}", r.error());
    }
}

PreferencesPanel::~PreferencesPanel() {}

void PreferencesPanel::apply_live_changes(EditorState& state) {
    // Apply font scale: ImGui::GetIO().FontGlobalScale = size / 14.0f
    ImGui::GetIO().FontGlobalScale = prefs_.editor_font_size / 14.0f;

    // Apply snap settings to editor state.
    state.snap_position = prefs_.position_snap;
    state.snap_rotation = prefs_.rotation_snap_deg;
    state.snap_scale = prefs_.scale_snap;

    // Camera base speed is used by SceneCamera::update (not stored in EditorState).
    // For now it's managed by the camera directly. Could be wired in future.
}

void PreferencesPanel::draw(EditorState& state) {
    if (!ImGui::Begin("Preferences")) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Display");
    if (ImGui::DragFloat("Editor Font Size", &prefs_.editor_font_size, 0.5f,
                         8.0f, 32.0f)) {
        // Apply immediately.
        ImGui::GetIO().FontGlobalScale = prefs_.editor_font_size / 14.0f;
    }

    ImGui::SeparatorText("Scene Viewport");
    ImGui::DragFloat("Camera Base Speed (m/s)", &prefs_.scene_camera_base_speed,
                     0.5f, 1.0f, 100.0f);

    ImGui::SeparatorText("Transform Snap");
    ImGui::DragFloat("Position Snap (m)", &prefs_.position_snap, 0.01f, 0.01f,
                     10.0f);
    ImGui::DragFloat("Rotation Snap (deg)", &prefs_.rotation_snap_deg, 1.0f,
                     1.0f, 180.0f);
    ImGui::DragFloat("Scale Snap", &prefs_.scale_snap, 0.01f, 0.01f, 10.0f);

    ImGui::SeparatorText("Autosave");
    ImGui::DragInt("Autosave Interval (sec)", &prefs_.autosave_interval_sec, 1,
                   0, 3600);
    ImGui::TextWrapped("Set to 0 to disable autosave.");

    ImGui::SeparatorText("Theme");
    ImGui::Checkbox("Dark Theme", &prefs_.dark_theme);

    ImGui::SeparatorText("Actions");
    if (ImGui::Button("Apply", ImVec2(100, 0))) {
        apply_live_changes(state);

        auto r = save_preferences(exe_dir_, prefs_);
        if (r.is_err()) {
            spdlog::error("[editor] failed to save preferences: {}", r.error());
        } else {
            spdlog::info("[editor] preferences saved");
        }
    }

    ImGui::End();
}

}  // namespace odyssey::editor
