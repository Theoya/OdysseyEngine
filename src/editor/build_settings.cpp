#include "editor/build_settings.h"

#include <pugixml.hpp>

#include <algorithm>

namespace odyssey::editor {

// Valid enum values for target and config.
static const std::vector<std::string> VALID_TARGETS = {
    "odyssey_shooter", "odyssey_fps", "odyssey_editor"
};
static const std::vector<std::string> VALID_CONFIGS = {
    "Debug", "Release", "RelWithDebInfo"
};

static bool is_valid_target(const std::string& target) {
    return std::find(VALID_TARGETS.begin(), VALID_TARGETS.end(), target) !=
           VALID_TARGETS.end();
}

static bool is_valid_config(const std::string& config) {
    return std::find(VALID_CONFIGS.begin(), VALID_CONFIGS.end(), config) !=
           VALID_CONFIGS.end();
}

Result<BuildSettings, std::string> load_build_settings(
    const std::filesystem::path& xml_path) {
    // Load and parse XML.
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_file(xml_path.c_str());
    if (!parse_result) {
        return Result<BuildSettings, std::string>::err(
            "XML parse error at offset " + std::to_string(parse_result.offset) +
            ": " + std::string(parse_result.description()));
    }

    // Get root element.
    auto root = doc.child("build_settings");
    if (!root) {
        return Result<BuildSettings, std::string>::err(
            "Missing root element <build_settings>");
    }

    // Validate and extract target (required).
    auto target_attr = root.attribute("target");
    if (!target_attr) {
        return Result<BuildSettings, std::string>::err(
            "Missing required attribute: target");
    }
    std::string target = target_attr.as_string();
    if (!is_valid_target(target)) {
        return Result<BuildSettings, std::string>::err(
            "Invalid target: " + target);
    }

    // Extract config (optional, default Release).
    std::string config = "Release";
    auto config_attr = root.attribute("config");
    if (config_attr) {
        config = config_attr.as_string();
        if (!is_valid_config(config)) {
            return Result<BuildSettings, std::string>::err(
                "Invalid config: " + config);
        }
    }

    // Extract output_dir (optional, default ../dist).
    std::string output_dir = "../dist";
    auto output_attr = root.attribute("output_dir");
    if (output_attr) {
        output_dir = output_attr.as_string();
        if (output_dir.empty()) {
            return Result<BuildSettings, std::string>::err(
                "output_dir cannot be empty");
        }
    }

    // Extract scenes (required, minOccurs=1).
    std::vector<std::string> scenes;
    auto scenes_elem = root.child("scenes");
    if (!scenes_elem) {
        return Result<BuildSettings, std::string>::err(
            "Missing required element: <scenes>");
    }
    for (auto scene : scenes_elem.children("scene")) {
        auto path_attr = scene.attribute("path");
        if (!path_attr) {
            return Result<BuildSettings, std::string>::err(
                "Scene element missing required attribute: path");
        }
        std::string path_str = path_attr.as_string();
        if (path_str.empty()) {
            return Result<BuildSettings, std::string>::err(
                "Scene path cannot be empty");
        }
        scenes.push_back(path_str);
    }
    if (scenes.empty()) {
        return Result<BuildSettings, std::string>::err(
            "No scenes defined in <scenes> element (minOccurs=1)");
    }

    // Extract defines (optional).
    std::unordered_map<std::string, std::string> defines;
    auto defines_elem = root.child("defines");
    if (defines_elem) {
        for (auto define : defines_elem.children("define")) {
            auto name_attr = define.attribute("name");
            auto value_attr = define.attribute("value");
            if (!name_attr || !value_attr) {
                return Result<BuildSettings, std::string>::err(
                    "Define element missing required attributes: name or value");
            }
            defines[name_attr.as_string()] = value_attr.as_string();
        }
    }

    BuildSettings result{target, config, output_dir, scenes, defines};
    return Result<BuildSettings, std::string>::ok(result);
}

Result<bool, std::string> save_build_settings(
    const std::filesystem::path& xml_path,
    const BuildSettings& settings) {
    // Create XML document.
    pugi::xml_document doc;

    // Add root element with attributes.
    auto root = doc.append_child("build_settings");
    root.append_attribute("target").set_value(settings.target.c_str());
    root.append_attribute("config").set_value(settings.config.c_str());
    root.append_attribute("output_dir").set_value(settings.output_dir.c_str());

    // Add scenes element and children.
    auto scenes_elem = root.append_child("scenes");
    for (const auto& scene : settings.scenes) {
        auto scene_elem = scenes_elem.append_child("scene");
        scene_elem.append_attribute("path").set_value(scene.c_str());
    }

    // Add defines element and children.
    if (!settings.defines.empty()) {
        auto defines_elem = root.append_child("defines");
        for (const auto& [name, value] : settings.defines) {
            auto define_elem = defines_elem.append_child("define");
            define_elem.append_attribute("name").set_value(name.c_str());
            define_elem.append_attribute("value").set_value(value.c_str());
        }
    }

    // Write to file.
    if (!doc.save_file(xml_path.c_str())) {
        return Result<bool, std::string>::err(
            "Failed to write build settings to " + xml_path.string());
    }

    return Result<bool, std::string>::ok(true);
}

}  // namespace odyssey::editor
