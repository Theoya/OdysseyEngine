#pragma once

#include "core/result.h"

#include <string>
#include <vector>
#include <filesystem>

namespace odyssey::editor {

// Editor preferences: persistent state loaded/saved as editor_prefs.xml
// in the executable directory.
struct EditorPrefs {
    std::vector<std::string> recent_scenes;  // up to 8 paths
    std::string active_layout;               // (reserved for future use)
};

// Load editor preferences from <exe_dir>/editor_prefs.xml.
// Returns EditorPrefs (default-empty if file doesn't exist).
// Returns Err only on parse errors or file I/O failures.
Result<EditorPrefs, std::string> load_editor_prefs(
    const std::filesystem::path& exe_dir);

// Save editor preferences to <exe_dir>/editor_prefs.xml.
Result<bool, std::string> save_editor_prefs(
    const std::filesystem::path& exe_dir,
    const EditorPrefs& prefs);

} // namespace odyssey::editor
