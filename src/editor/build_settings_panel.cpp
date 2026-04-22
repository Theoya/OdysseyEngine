#include "editor/build_settings_panel.h"
#include "editor/editor.h"
#include "editor/file_dialog_win32.h"

#include <imgui.h>

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace odyssey::editor {

BuildSettingsPanel::BuildSettingsPanel()
    : settings_{
        .target = "odyssey_shooter",
        .config = "Release",
        .output_dir = "../dist",
        .scenes = {},
        .defines = {}
    },
      target_radio_selected_(0),
      config_selected_(1) {}

BuildSettingsPanel::~BuildSettingsPanel() {}

void BuildSettingsPanel::apply_ui_to_settings() {
    const std::vector<std::string> targets = {
        "odyssey_shooter", "odyssey_fps", "odyssey_editor"
    };
    const std::vector<std::string> configs = {
        "Debug", "Release", "RelWithDebInfo"
    };

    if (target_radio_selected_ < static_cast<int>(targets.size())) {
        settings_.target = targets[target_radio_selected_];
    }
    if (config_selected_ < static_cast<int>(configs.size())) {
        settings_.config = configs[config_selected_];
    }
}

void BuildSettingsPanel::apply_settings_to_ui() {
    const std::vector<std::string> targets = {
        "odyssey_shooter", "odyssey_fps", "odyssey_editor"
    };
    const std::vector<std::string> configs = {
        "Debug", "Release", "RelWithDebInfo"
    };

    auto it = std::find(targets.begin(), targets.end(), settings_.target);
    if (it != targets.end()) {
        target_radio_selected_ = std::distance(targets.begin(), it);
    }

    it = std::find(configs.begin(), configs.end(), settings_.config);
    if (it != configs.end()) {
        config_selected_ = std::distance(configs.begin(), it);
    }
}

void BuildSettingsPanel::draw(EditorState& state) {
    if (!ImGui::Begin("Build Settings")) {
        ImGui::End();
        return;
    }

    apply_ui_to_settings();

    // Target game selection.
    ImGui::SeparatorText("Target");
    const char* targets[] = {"odyssey_shooter", "odyssey_fps", "odyssey_editor"};
    for (int i = 0; i < 3; i++) {
        if (ImGui::RadioButton(targets[i], &target_radio_selected_, i)) {
            apply_ui_to_settings();
        }
    }

    // Scenes in build.
    ImGui::SeparatorText("Scenes");
    ImGui::Text("Scenes to include:");
    for (size_t i = 0; i < settings_.scenes.size(); i++) {
        ImGui::BulletText("%s", settings_.scenes[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(("Remove##" + std::to_string(i)).c_str())) {
            settings_.scenes.erase(settings_.scenes.begin() + i);
        }
    }

    if (ImGui::Button("Add Scene")) {
        auto result = open_scene_dialog();
        if (result.has_value()) {
            settings_.scenes.push_back(result.value().string());
        }
    }

    // Build configuration.
    ImGui::SeparatorText("Configuration");
    const char* configs[] = {"Debug", "Release", "RelWithDebInfo"};
    ImGui::Combo("Config", &config_selected_, configs, 3);
    apply_ui_to_settings();

    // Output directory.
    char output_dir_buf[512];
    std::strcpy(output_dir_buf, settings_.output_dir.c_str());
    if (ImGui::InputText("Output Dir##build", output_dir_buf, sizeof(output_dir_buf))) {
        settings_.output_dir = output_dir_buf;
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Output Folder")) {
        // Use ShellExecuteW to open the folder in Explorer.
        std::wstring output_dir_w(settings_.output_dir.begin(),
                                   settings_.output_dir.end());
        ShellExecuteW(nullptr, L"explore", output_dir_w.c_str(), nullptr,
                      nullptr, SW_SHOW);
    }

    // Build button.
    ImGui::SeparatorText("Build");
    if (settings_.scenes.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No scenes defined");
    } else if (!build_runner_.is_running()) {
        if (ImGui::Button("Build", ImVec2(100, 0))) {
            // Create build directory if it doesn't exist.
            std::filesystem::create_directories("build");
            auto r = build_runner_.run_build("build", settings_.config,
                                             settings_.target);
            if (r.is_err()) {
                spdlog::error("[editor] build failed to start: {}", r.error());
            } else {
                show_build_output_ = true;
            }
        }
    } else {
        // Show spinner while building.
        build_progress_spinner_angle_ += 5.0f;
        ImGui::TextWrapped("Building %s (%s)...", settings_.target.c_str(),
                           settings_.config.c_str());
        ImGui::ProgressBar(0.5f, ImVec2(-1, 0), "In Progress");
    }

    // Show build output in collapsing header.
    if (show_build_output_ && ImGui::CollapsingHeader("Build Output")) {
        auto build_result = build_runner_.get_result();
        ImGui::TextWrapped("State: %s, Exit: %d",
            build_result.state == BuildState::Idle ? "Idle" :
            build_result.state == BuildState::Running ? "Running" :
            build_result.state == BuildState::Success ? "Success" :
            build_result.state == BuildState::Failed ? "Failed" : "TimedOut",
            build_result.exit_code);

        if (!build_result.stdout_log.empty()) {
            ImGui::TextWrapped("--- STDOUT ---");
            ImGui::TextWrapped("%s", build_result.stdout_log.c_str());
        }
        if (!build_result.stderr_log.empty()) {
            ImGui::TextWrapped("--- STDERR ---");
            ImGui::TextWrapped("%s", build_result.stderr_log.c_str());
        }
    }

    ImGui::End();
}

}  // namespace odyssey::editor
