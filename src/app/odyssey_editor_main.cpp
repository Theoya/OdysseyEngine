// ---------------------------------------------------------------------------
// odyssey_editor_main.cpp
// Entry point for the `odyssey_editor` executable.
//
// Phase 1 scope:
//   - Parse a single optional positional argument: the scene path to open.
//     (Default: demo/showcase/showcase.scene.xml)
//   - Start in Edit mode.
//   - Run the Editor main loop until the window is closed.
//
// The Editor class manages its own GLFW window and Vulkan context, so this
// file is kept deliberately thin.
// ---------------------------------------------------------------------------

#include "editor/editor.h"
#include "editor/mode_enum.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    std::filesystem::path scene_path = "demo/showcase/showcase.scene.xml";
    if (argc >= 2) {
        scene_path = argv[1];
    }

    if (!std::filesystem::exists(scene_path)) {
        spdlog::warn("Scene file not found: {} — opening editor without a scene.",
                     scene_path.string());
        scene_path.clear();
    }

    odyssey::editor::Editor editor;
    auto init = editor.initialize(scene_path);
    if (init.is_err()) {
        spdlog::error("Editor initialize failed: {}", init.error());
        return 1;
    }
    spdlog::info("Editor started in {} mode",
                 std::string(odyssey::editor::mode_label(odyssey::editor::Mode::Edit)));

    editor.run();
    editor.shutdown();
    return 0;
}
