#pragma once

#include "core/result.h"

#include <filesystem>
#include <string>

namespace odyssey::editor {

// Pure data: editor application preferences.
// Persisted to <exe_dir>/editor_preferences.xml (separate from session prefs).
struct Preferences {
    float editor_font_size = 14.0f;      // ImGui font scale base
    float scene_camera_base_speed = 10.0f;  // meters/second in Edit mode
    float position_snap = 0.25f;         // meters
    float rotation_snap_deg = 15.0f;     // degrees
    float scale_snap = 0.1f;             // scale units
    int autosave_interval_sec = 0;       // 0 = off, >0 = interval in seconds
    bool dark_theme = true;              // for future light-theme toggle
};

// Load preferences from <exe_dir>/editor_preferences.xml.
// Returns default Preferences if file doesn't exist.
// Returns error if XML is malformed.
Result<Preferences, std::string> load_preferences(
    const std::filesystem::path& exe_dir);

// Save preferences to <exe_dir>/editor_preferences.xml.
// Returns ok(true) on success, error string on I/O failure.
Result<bool, std::string> save_preferences(
    const std::filesystem::path& exe_dir,
    const Preferences& prefs);

}  // namespace odyssey::editor
