#include "editor/preferences.h"

#include <pugixml.hpp>

namespace odyssey::editor {

Result<Preferences, std::string> load_preferences(
    const std::filesystem::path& exe_dir) {
    auto xml_path = exe_dir / "editor_preferences.xml";

    // If file doesn't exist, return defaults.
    if (!std::filesystem::exists(xml_path)) {
        return Result<Preferences, std::string>::ok(Preferences{});
    }

    // Parse XML.
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_file(xml_path.c_str());
    if (!parse_result) {
        return Result<Preferences, std::string>::err(
            "XML parse error: " + std::string(parse_result.description()));
    }

    auto root = doc.child("preferences");
    if (!root) {
        // File exists but is empty or malformed; return defaults.
        return Result<Preferences, std::string>::ok(Preferences{});
    }

    Preferences prefs;
    prefs.editor_font_size =
        root.child("editor_font_size").text().as_float(14.0f);
    prefs.scene_camera_base_speed =
        root.child("scene_camera_base_speed").text().as_float(10.0f);
    prefs.position_snap =
        root.child("position_snap").text().as_float(0.25f);
    prefs.rotation_snap_deg =
        root.child("rotation_snap_deg").text().as_float(15.0f);
    prefs.scale_snap =
        root.child("scale_snap").text().as_float(0.1f);
    prefs.autosave_interval_sec =
        root.child("autosave_interval_sec").text().as_int(0);
    prefs.dark_theme =
        root.child("dark_theme").text().as_bool(true);

    return Result<Preferences, std::string>::ok(prefs);
}

Result<bool, std::string> save_preferences(
    const std::filesystem::path& exe_dir,
    const Preferences& prefs) {
    auto xml_path = exe_dir / "editor_preferences.xml";

    pugi::xml_document doc;
    auto root = doc.append_child("preferences");

    auto font_elem = root.append_child("editor_font_size");
    font_elem.text().set(prefs.editor_font_size);

    auto speed_elem = root.append_child("scene_camera_base_speed");
    speed_elem.text().set(prefs.scene_camera_base_speed);

    auto pos_snap_elem = root.append_child("position_snap");
    pos_snap_elem.text().set(prefs.position_snap);

    auto rot_snap_elem = root.append_child("rotation_snap_deg");
    rot_snap_elem.text().set(prefs.rotation_snap_deg);

    auto scale_snap_elem = root.append_child("scale_snap");
    scale_snap_elem.text().set(prefs.scale_snap);

    auto autosave_elem = root.append_child("autosave_interval_sec");
    autosave_elem.text().set(prefs.autosave_interval_sec);

    auto theme_elem = root.append_child("dark_theme");
    theme_elem.text().set(prefs.dark_theme);

    if (!doc.save_file(xml_path.c_str())) {
        return Result<bool, std::string>::err(
            "Failed to write preferences to " + xml_path.string());
    }

    return Result<bool, std::string>::ok(true);
}

}  // namespace odyssey::editor
