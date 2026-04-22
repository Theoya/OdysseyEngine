#pragma once

#include "core/result.h"

#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace odyssey::editor {

// State enum for async build operations.
enum class BuildState {
    Idle,          // No build in progress
    Running,       // Build is executing
    Success,       // Build completed with exit code 0
    Failed,        // Build completed with non-zero exit code
    TimedOut       // Build killed after 10-minute timeout
};

// Pure data: result of a build operation.
struct BuildResult {
    BuildState state;
    int exit_code;           // 0 = success
    std::string stdout_log;  // full captured stdout
    std::string stderr_log;  // full captured stderr
};

class BuildRunner {
public:
    BuildRunner();
    ~BuildRunner();

    BuildRunner(const BuildRunner&) = delete;
    BuildRunner& operator=(const BuildRunner&) = delete;

    // Asynchronously run: cmake --build <build_dir> --config <cfg> --target <target>
    // Returns BuildInProgress, or an error if spawn fails (eg. cmake not found).
    // Caller must poll get_result() to check completion; timeout is 10 minutes.
    // Pipes stdout/stderr to the log (via spdlog) in real time.
    Result<bool, std::string> run_build(
        const std::filesystem::path& build_dir,
        const std::string& config,
        const std::string& target);

    // Check if a build is currently running.
    bool is_running() const;

    // Get the current build state and logs. Returns immediately.
    BuildResult get_result() const;

    // Kill the build process. Safe to call even if no build is running.
    // Returns ok(true) on success or already-stopped, error if kill fails.
    Result<bool, std::string> kill_build();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace odyssey::editor
