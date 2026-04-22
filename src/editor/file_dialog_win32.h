#pragma once

#include <filesystem>
#include <optional>

namespace odyssey::editor {

// Windows-native file dialogs using GetOpenFileNameW and GetSaveFileNameW.
// All paths returned/accepted are UTF-8 std::string on the C++ side, but
// internally converted to/from wide chars for Win32 APIs.

// Open a scene file (*.scene.xml). Returns nullopt if user cancelled.
std::optional<std::filesystem::path> open_scene_dialog();

// Save a scene file (*.scene.xml). Returns nullopt if user cancelled.
// If the user selects an existing file, they will be prompted to overwrite.
std::optional<std::filesystem::path> save_scene_dialog(
    const std::filesystem::path& suggested_name = "");

} // namespace odyssey::editor
