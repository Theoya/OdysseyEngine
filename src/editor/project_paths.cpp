#include "editor/project_paths.h"

namespace odyssey::editor {

Result<ProjectPaths> resolve_project_paths(
    const std::filesystem::path& exe_dir,
    const std::filesystem::path& cli_scene_arg)
{
    if (exe_dir.empty()) {
        return Result<ProjectPaths>::err("exe_dir is empty");
    }
    if (!exe_dir.is_absolute()) {
        return Result<ProjectPaths>::err("exe_dir is not absolute: " + exe_dir.string());
    }

    ProjectPaths p;
    p.exe_dir      = exe_dir;
    p.project_root = exe_dir;
    p.asset_root   = exe_dir / "demo" / "showcase";
    p.layouts_dir  = exe_dir / "layouts";

    if (cli_scene_arg.empty()) {
        p.showcase_scene = exe_dir / "demo" / "showcase" / "showcase.scene.xml";
    } else if (cli_scene_arg.is_absolute()) {
        p.showcase_scene = cli_scene_arg;
    } else {
        p.showcase_scene = exe_dir / cli_scene_arg;
    }
    return Result<ProjectPaths>::ok(p);
}

} // namespace odyssey::editor
