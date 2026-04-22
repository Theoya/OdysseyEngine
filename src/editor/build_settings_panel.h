#pragma once

#include "editor/panel.h"
#include "editor/build_settings.h"
#include "editor/build_runner.h"

namespace odyssey::editor {

struct EditorState;

// UI panel for build configuration: target, scenes, config, output dir, and build button.
class BuildSettingsPanel : public Panel {
public:
    BuildSettingsPanel();
    ~BuildSettingsPanel() override;

    const std::string& name() const override {
        static const std::string NAME = "Build Settings";
        return NAME;
    }

    void draw(EditorState& state) override;

private:
    BuildSettings settings_;
    BuildRunner build_runner_;

    // UI state
    int target_radio_selected_ = 0;  // 0=shooter, 1=fps, 2=editor
    int config_selected_ = 1;        // 0=Debug, 1=Release, 2=RelWithDebInfo
    bool show_build_output_ = false;
    float build_progress_spinner_angle_ = 0.0f;

    // Helper: sync radio/dropdown to settings_ struct.
    void apply_ui_to_settings();
    // Helper: sync settings_ struct to radio/dropdown.
    void apply_settings_to_ui();
};

}  // namespace odyssey::editor
