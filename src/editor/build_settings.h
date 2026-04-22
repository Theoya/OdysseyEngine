#pragma once

#include "core/result.h"

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

namespace odyssey::editor {

// Pure data struct: represents parsed <build_settings> XML.
struct BuildSettings {
    std::string target;  // odyssey_shooter | odyssey_fps | odyssey_editor
    std::string config;  // Debug | Release | RelWithDebInfo
    std::string output_dir;  // relative or absolute path
    std::vector<std::string> scenes;  // list of scene XML paths
    std::unordered_map<std::string, std::string> defines;  // build-time defines
};

// Load build settings from XML file. Returns BuildSettings on success,
// error string if XML is malformed, missing required attrs, or has invalid enum values.
Result<BuildSettings, std::string> load_build_settings(
    const std::filesystem::path& xml_path);

// Save build settings to XML file. Returns ok(true) on success,
// error string if write fails.
Result<bool, std::string> save_build_settings(
    const std::filesystem::path& xml_path,
    const BuildSettings& settings);

}  // namespace odyssey::editor
