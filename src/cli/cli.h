#pragma once

#include <string>
#include <vector>
#include <functional>

namespace odyssey::cli {

/// CLI command result — describes what happened and what to do next.
struct CommandResult {
    int exit_code = 0;
    std::string output;
    bool should_run_engine = false;  // true only for "run" command
    std::string scene_path;          // populated by "run --scene <path>"
};

/// Parse and execute CLI commands.
/// Returns CommandResult describing the outcome and whether to launch the engine.
CommandResult run_cli(int argc, char* argv[]);

/// Individual command implementations — each is pure or near-pure,
/// operating only on the arguments supplied.
namespace commands {

/// Build the project.  If release is true, uses Release config; otherwise Debug.
CommandResult cmd_build(bool release);

/// Launch the engine with an optional scene override.
CommandResult cmd_run(const std::string& scene_path);

/// List all .nadir behavior files found under behavior_dir.
CommandResult cmd_nadir_list(const std::string& behavior_dir);

/// Compile a single .nadir file using the Nadir compiler.
CommandResult cmd_nadir_compile(const std::string& path, const std::string& lib_dir);

/// Validate a single .nadir file (compile without emitting artifacts).
CommandResult cmd_nadir_validate(const std::string& path, const std::string& lib_dir);

/// Run tests matching filter: "unit", "pipeline", or "all".
CommandResult cmd_test(const std::string& filter);

/// List all scene files found under scene_dir.
CommandResult cmd_scene_list(const std::string& scene_dir);

/// Validate a scene file against the engine schema.
CommandResult cmd_scene_validate(const std::string& path);

} // namespace commands
} // namespace odyssey::cli
