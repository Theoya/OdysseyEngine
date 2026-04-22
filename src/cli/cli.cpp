#include "cli/cli.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace odyssey::cli {

// ---------------------------------------------------------------------------
// Command implementations
// ---------------------------------------------------------------------------

namespace commands {

CommandResult cmd_build(bool release) {
    CommandResult result;
    std::string config = release ? "Release" : "Debug";

    std::ostringstream oss;
    oss << "[build] Configuration: " << config << "\n";
    oss << "[build] Running CMake configure + build...\n";

    // Construct the build command.  We rely on CMake presets or a simple
    // invocation.  In Phase 1 this just reports what *would* happen.
    std::string cmake_cmd =
        "cmake --build build --config " + config + " --parallel";

    oss << "[build] > " << cmake_cmd << "\n";

    int rc = std::system(cmake_cmd.c_str());
    if (rc != 0) {
        oss << "[build] Build failed with exit code " << rc << "\n";
        result.exit_code = 1;
    } else {
        oss << "[build] Build succeeded.\n";
    }

    result.output = oss.str();
    return result;
}

CommandResult cmd_run(const std::string& scene_path) {
    CommandResult result;
    result.should_run_engine = true;
    result.scene_path = scene_path;

    std::ostringstream oss;
    oss << "[run] Launching engine";
    if (!scene_path.empty()) {
        oss << " with scene: " << scene_path;
    }
    oss << "\n";

    result.output = oss.str();
    return result;
}

CommandResult cmd_nadir_list(const std::string& behavior_dir) {
    CommandResult result;
    std::ostringstream oss;

    fs::path dir(behavior_dir);
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        oss << "[nadir] Behavior directory not found: " << behavior_dir << "\n";
        result.exit_code = 1;
        result.output = oss.str();
        return result;
    }

    oss << "[nadir] Behavior files in " << behavior_dir << ":\n";
    std::vector<std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".nadir") {
            files.push_back(entry.path().string());
        }
    }

    std::sort(files.begin(), files.end());

    if (files.empty()) {
        oss << "  (none)\n";
    } else {
        for (const auto& f : files) {
            oss << "  " << f << "\n";
        }
    }

    result.output = oss.str();
    return result;
}

CommandResult cmd_nadir_compile(const std::string& path,
                                const std::string& lib_dir) {
    CommandResult result;
    std::ostringstream oss;

    fs::path nadir_path(path);
    if (!fs::exists(nadir_path)) {
        oss << "[nadir] File not found: " << path << "\n";
        result.exit_code = 1;
        result.output = oss.str();
        return result;
    }

    oss << "[nadir] Compiling: " << path << "\n";

    // In the integrated build this calls:
    //   nadir::compile_nadir_file(nadir_path, lib_dir)
    // For Phase 1 we report intent; actual compilation is wired up once
    // the nadir module is linked.
    oss << "[nadir] Compile requested for " << nadir_path.filename().string()
        << " (lib_dir=" << lib_dir << ")\n";
    oss << "[nadir] Note: full compilation requires linked Nadir module.\n";

    result.output = oss.str();
    return result;
}

CommandResult cmd_nadir_validate(const std::string& path,
                                 const std::string& lib_dir) {
    CommandResult result;
    std::ostringstream oss;

    fs::path nadir_path(path);
    if (!fs::exists(nadir_path)) {
        oss << "[nadir] File not found: " << path << "\n";
        result.exit_code = 1;
        result.output = oss.str();
        return result;
    }

    oss << "[nadir] Validating: " << path << "\n";

    // Same as compile but only reports success / failure.
    oss << "[nadir] Validation requested for "
        << nadir_path.filename().string()
        << " (lib_dir=" << lib_dir << ")\n";
    oss << "[nadir] Note: full validation requires linked Nadir module.\n";

    result.output = oss.str();
    return result;
}

CommandResult cmd_test(const std::string& filter) {
    CommandResult result;
    std::ostringstream oss;

    oss << "[test] Running tests";
    if (filter == "unit") {
        oss << " (unit only)\n";
    } else if (filter == "pipeline") {
        oss << " (pipeline only)\n";
    } else {
        oss << " (all)\n";
    }

    // Delegate to CTest with a label filter.
    std::string ctest_cmd = "ctest --test-dir build --output-on-failure";
    if (filter == "unit") {
        ctest_cmd += " -L unit";
    } else if (filter == "pipeline") {
        ctest_cmd += " -L pipeline";
    }

    oss << "[test] > " << ctest_cmd << "\n";

    int rc = std::system(ctest_cmd.c_str());
    if (rc != 0) {
        oss << "[test] Some tests failed (exit code " << rc << ")\n";
        result.exit_code = 1;
    } else {
        oss << "[test] All tests passed.\n";
    }

    result.output = oss.str();
    return result;
}

CommandResult cmd_scene_list(const std::string& scene_dir) {
    CommandResult result;
    std::ostringstream oss;

    fs::path dir(scene_dir);
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        oss << "[scene] Scene directory not found: " << scene_dir << "\n";
        result.exit_code = 1;
        result.output = oss.str();
        return result;
    }

    oss << "[scene] Scene files in " << scene_dir << ":\n";
    std::vector<std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() &&
            (entry.path().extension() == ".xml" ||
             entry.path().extension() == ".scene")) {
            files.push_back(entry.path().string());
        }
    }

    std::sort(files.begin(), files.end());

    if (files.empty()) {
        oss << "  (none)\n";
    } else {
        for (const auto& f : files) {
            oss << "  " << f << "\n";
        }
    }

    result.output = oss.str();
    return result;
}

CommandResult cmd_scene_validate(const std::string& path) {
    CommandResult result;
    std::ostringstream oss;

    fs::path scene_path(path);
    if (!fs::exists(scene_path)) {
        oss << "[scene] File not found: " << path << "\n";
        result.exit_code = 1;
        result.output = oss.str();
        return result;
    }

    oss << "[scene] Validating: " << path << "\n";

    // Phase 1 placeholder — full XML schema validation is wired up
    // once pugixml and the scene schema are integrated.
    oss << "[scene] Basic file existence check passed.\n";
    oss << "[scene] Note: full XML schema validation requires linked scene module.\n";

    result.output = oss.str();
    return result;
}

// ---------------------------------------------------------------------------
// cmd_assets_bindless_stats
// ---------------------------------------------------------------------------

CommandResult cmd_assets_bindless_stats(uint32_t used, uint32_t total) {
    // Pure function: stats are passed in from the live registry rather than
    // queried directly, keeping this command testable without a GPU device.
    CommandResult result;
    std::ostringstream oss;

    uint32_t free_slots = total > used ? (total - used) : 0u;
    float occupancy_pct  = total > 0 ? (static_cast<float>(used) / static_cast<float>(total)) * 100.0f : 0.0f;
    float fragment_pct   = 0.0f; // Phase 6: free list is LIFO; fragmentation = 0 by construction

    oss << "[assets] Bindless texture registry stats\n";
    oss << "  Capacity      : " << total << " slots\n";
    oss << "  Used          : " << used  << " slots";
    if (total > 0) {
        oss << " (" << static_cast<int>(occupancy_pct) << "%)";
    }
    oss << "\n";
    oss << "  Free          : " << free_slots << " slots\n";
    oss << "  Fragmentation : " << static_cast<int>(fragment_pct) << "% (LIFO free-list, always 0)\n";
    oss << "  Slot 0        : [reserved — magenta sentinel]\n";
    oss << "  Note: run with --verbose for per-slot resident list (Phase 7)\n";

    result.output = oss.str();
    return result;
}

// ---------------------------------------------------------------------------
// cmd_assets_texture_count
// ---------------------------------------------------------------------------

CommandResult cmd_assets_texture_count(uint32_t used) {
    // Pure function: used count passed in from live registry.
    // Slot 0 (sentinel) is included in `used` — subtract 1 for authoring budget.
    CommandResult result;
    std::ostringstream oss;
    uint32_t authoring_count = used > 0 ? used - 1u : 0u; // exclude sentinel
    oss << authoring_count << "\n";
    result.output = oss.str();
    return result;
}

} // namespace commands

// ---------------------------------------------------------------------------
// CLI entry point
// ---------------------------------------------------------------------------

CommandResult run_cli(int argc, char* argv[]) {
    CommandResult result;

    CLI::App app{"OdysseyEngine - GPU-maximalist 3D engine"};
    app.require_subcommand(1);

    // -----------------------------------------------------------------------
    // build
    // -----------------------------------------------------------------------
    bool build_release = false;
    auto* build_cmd = app.add_subcommand("build", "Build the engine");
    build_cmd->add_flag("--release", build_release,
                        "Build in Release mode (default: Debug)");

    // -----------------------------------------------------------------------
    // run
    // -----------------------------------------------------------------------
    std::string scene_path;
    auto* run_cmd = app.add_subcommand("run", "Run the engine");
    run_cmd->add_option("--scene", scene_path, "Path to scene file to load");

    // -----------------------------------------------------------------------
    // nadir (sub-subcommands)
    // -----------------------------------------------------------------------
    auto* nadir_cmd = app.add_subcommand("nadir", "Nadir behavior system");
    nadir_cmd->require_subcommand(1);

    std::string behavior_dir = "behaviors/shaders";
    std::string lib_dir = "behaviors/lib";

    auto* nadir_list_cmd =
        nadir_cmd->add_subcommand("list", "List behavior files");
    nadir_list_cmd->add_option("--dir", behavior_dir,
                               "Behavior directory (default: behaviors/shaders)");

    std::string nadir_compile_path;
    auto* nadir_compile_cmd =
        nadir_cmd->add_subcommand("compile", "Compile a .nadir file");
    nadir_compile_cmd->add_option("path", nadir_compile_path,
                                  "Path to .nadir file")
        ->required();
    nadir_compile_cmd->add_option("--lib-dir", lib_dir,
                                  "Library include directory");

    std::string nadir_validate_path;
    auto* nadir_validate_cmd =
        nadir_cmd->add_subcommand("validate", "Validate a .nadir file");
    nadir_validate_cmd->add_option("path", nadir_validate_path,
                                   "Path to .nadir file")
        ->required();
    nadir_validate_cmd->add_option("--lib-dir", lib_dir,
                                   "Library include directory");

    // -----------------------------------------------------------------------
    // test
    // -----------------------------------------------------------------------
    bool test_unit = false;
    bool test_pipeline = false;
    bool test_all = false;
    auto* test_cmd = app.add_subcommand("test", "Run tests");
    test_cmd->add_flag("--unit", test_unit, "Run unit tests only");
    test_cmd->add_flag("--pipeline", test_pipeline,
                       "Run pipeline tests only");
    test_cmd->add_flag("--all", test_all, "Run all tests");

    // -----------------------------------------------------------------------
    // scene (sub-subcommands)
    // -----------------------------------------------------------------------
    auto* scene_cmd = app.add_subcommand("scene", "Scene management");
    scene_cmd->require_subcommand(1);

    std::string scene_list_dir = "scenes";
    auto* scene_list_cmd =
        scene_cmd->add_subcommand("list", "List scene files");
    scene_list_cmd->add_option("--dir", scene_list_dir,
                               "Scene directory (default: scenes)");

    std::string scene_validate_path;
    auto* scene_validate_cmd =
        scene_cmd->add_subcommand("validate", "Validate a scene file");
    scene_validate_cmd->add_option("path", scene_validate_path,
                                   "Path to scene file")
        ->required();

    // -----------------------------------------------------------------------
    // Parse
    // -----------------------------------------------------------------------
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        // CLI11 handles --help, parse errors, etc.
        std::ostringstream oss;
        // Capture CLI11 exit output
        result.exit_code = app.exit(e);
        return result;
    }

    // -----------------------------------------------------------------------
    // Dispatch
    // -----------------------------------------------------------------------
    if (build_cmd->parsed()) {
        result = commands::cmd_build(build_release);
    } else if (run_cmd->parsed()) {
        result = commands::cmd_run(scene_path);
    } else if (nadir_list_cmd->parsed()) {
        result = commands::cmd_nadir_list(behavior_dir);
    } else if (nadir_compile_cmd->parsed()) {
        result = commands::cmd_nadir_compile(nadir_compile_path, lib_dir);
    } else if (nadir_validate_cmd->parsed()) {
        result = commands::cmd_nadir_validate(nadir_validate_path, lib_dir);
    } else if (test_cmd->parsed()) {
        std::string filter = "all";
        if (test_unit) {
            filter = "unit";
        } else if (test_pipeline) {
            filter = "pipeline";
        }
        result = commands::cmd_test(filter);
    } else if (scene_list_cmd->parsed()) {
        result = commands::cmd_scene_list(scene_list_dir);
    } else if (scene_validate_cmd->parsed()) {
        result = commands::cmd_scene_validate(scene_validate_path);
    }

    return result;
}

} // namespace odyssey::cli
