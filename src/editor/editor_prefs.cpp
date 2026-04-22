#include "editor/editor_prefs.h"

#include <pugixml.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace odyssey::editor {

Result<EditorPrefs, std::string> load_editor_prefs(
    const std::filesystem::path& exe_dir) {

    EditorPrefs prefs;
    std::filesystem::path prefs_path = exe_dir / "editor_prefs.xml";

    // If file doesn't exist, return a default EditorPrefs (empty recent list).
    if (!std::filesystem::exists(prefs_path)) {
        return Result<EditorPrefs, std::string>::ok(prefs);
    }

    // File exists; parse it.
    pugi::xml_document doc;
    pugi::xml_parse_result load_result = doc.load_file(prefs_path.c_str());
    if (!load_result) {
        return Result<EditorPrefs, std::string>::err(std::string("XML parse error: ") + load_result.description());
    }

    // Extract recent_scenes list
    pugi::xml_node root = doc.child("editor_prefs");
    if (root) {
        pugi::xml_node recent = root.child("recent_scenes");
        if (recent) {
            for (pugi::xml_node scene : recent.children("scene")) {
                const char* path_attr = scene.attribute("path").value();
                if (path_attr && path_attr[0] != '\0') {
                    prefs.recent_scenes.push_back(path_attr);
                }
            }
        }
        const char* layout = root.attribute("active_layout").value();
        if (layout && layout[0] != '\0') {
            prefs.active_layout = layout;
        }
    }

    // Cap at 8 — anything more is discarded.
    if (prefs.recent_scenes.size() > 8) {
        prefs.recent_scenes.resize(8);
    }

    return Result<EditorPrefs, std::string>::ok(prefs);
}

Result<bool, std::string> save_editor_prefs(
    const std::filesystem::path& exe_dir,
    const EditorPrefs& prefs) {

    pugi::xml_document doc;

    // Create root element
    pugi::xml_node root = doc.append_child("editor_prefs");
    if (!prefs.active_layout.empty()) {
        root.append_attribute("active_layout") = prefs.active_layout.c_str();
    }

    // Create recent_scenes list
    pugi::xml_node recent = root.append_child("recent_scenes");
    for (const auto& scene_path : prefs.recent_scenes) {
        pugi::xml_node scene = recent.append_child("scene");
        scene.append_attribute("path") = scene_path.c_str();
    }

    // Write to file
    std::filesystem::path prefs_path = exe_dir / "editor_prefs.xml";
    if (!doc.save_file(prefs_path.c_str())) {
        return Result<bool, std::string>::err("Failed to write " + prefs_path.string());
    }

    return Result<bool, std::string>::ok(true);
}

} // namespace odyssey::editor
