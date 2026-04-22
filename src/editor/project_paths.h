#pragma once
#include "core/result.h"
#include <filesystem>
#include <string>

namespace odyssey::editor {

struct ProjectPaths {
    std::filesystem::path exe_dir;        // absolute
    std::filesystem::path project_root;   // = exe_dir
    std::filesystem::path showcase_scene; // = exe_dir / "demo/showcase/showcase.scene.xml" OR CLI abs override
    std::filesystem::path asset_root;     // = exe_dir / "demo/showcase"
    std::filesystem::path layouts_dir;    // = exe_dir / "layouts"
};

// Pure function. Returns Err if exe_dir is empty or relative.
// cli_scene_arg may be empty, relative, or absolute; relative is resolved
// against exe_dir; absolute is taken verbatim (with existence NOT checked —
// caller is responsible for presenting a warn if the file is missing).
Result<ProjectPaths> resolve_project_paths(
    const std::filesystem::path& exe_dir,
    const std::filesystem::path& cli_scene_arg);

} // namespace odyssey::editor
