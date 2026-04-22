#pragma once

#include "editor/panel.h"
#include "editor/preferences.h"

namespace odyssey::editor {

struct EditorState;

// Dockable window for editor preferences: font size, camera speed, snap settings, etc.
class PreferencesPanel : public Panel {
public:
    explicit PreferencesPanel(const std::filesystem::path& exe_dir);
    ~PreferencesPanel() override;

    const std::string& name() const override {
        static const std::string NAME = "Preferences";
        return NAME;
    }

    void draw(EditorState& state) override;

    // Get current preferences (may not be saved to disk yet).
    const Preferences& get_preferences() const { return prefs_; }

    // Apply live changes (font scale, camera speed, snap state).
    void apply_live_changes(EditorState& state);

private:
    Preferences prefs_;
    std::filesystem::path exe_dir_;
};

}  // namespace odyssey::editor
