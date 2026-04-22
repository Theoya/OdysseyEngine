#include "editor/build_runner.h"

#define NOMINMAX
#include <windows.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <chrono>
#include <thread>
#include <mutex>

namespace odyssey::editor {

struct BuildRunner::Impl {
    PROCESS_INFORMATION pi{};
    HANDLE stdout_read = INVALID_HANDLE_VALUE;
    HANDLE stdout_write = INVALID_HANDLE_VALUE;
    HANDLE stderr_read = INVALID_HANDLE_VALUE;
    HANDLE stderr_write = INVALID_HANDLE_VALUE;
    HANDLE process_handle = INVALID_HANDLE_VALUE;

    std::thread monitor_thread;
    mutable std::mutex result_mutex;
    BuildResult result{BuildState::Idle, 0, "", ""};
    std::chrono::steady_clock::time_point start_time;

    bool is_running_unsafe() const {
        if (process_handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD exit_code = 0;
        if (GetExitCodeProcess(process_handle, &exit_code)) {
            return exit_code == STILL_ACTIVE;
        }
        return false;
    }
};

BuildRunner::BuildRunner() : impl_(std::make_unique<Impl>()) {}

BuildRunner::~BuildRunner() {
    kill_build();
    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }
}

// Helper: read all available data from a pipe into a string.
static std::string read_pipe(HANDLE pipe_read) {
    std::string output;
    const size_t BUF_SIZE = 4096;
    char buf[BUF_SIZE];
    DWORD bytes_read = 0;

    while (ReadFile(pipe_read, buf, BUF_SIZE - 1, &bytes_read, nullptr) &&
           bytes_read > 0) {
        buf[bytes_read] = '\0';
        output.append(buf);
    }
    return output;
}

// Monitor thread: collects pipes and waits for process completion.
// Forward declaration needed because Impl is private.
static void monitor_process_impl(
    HANDLE process_handle,
    HANDLE stdout_read,
    HANDLE stderr_read,
    std::mutex& result_mutex,
    BuildResult& result) {
    // Wait for process with 10-minute timeout.
    const DWORD TIMEOUT_MS = 10 * 60 * 1000;  // 600 seconds
    DWORD wait_result = WaitForSingleObject(process_handle, TIMEOUT_MS);

    // Collect stdout/stderr.
    std::string stdout_log = read_pipe(stdout_read);
    std::string stderr_log = read_pipe(stderr_read);

    // Determine result.
    std::lock_guard<std::mutex> lock(result_mutex);
    if (wait_result == WAIT_TIMEOUT) {
        result.state = BuildState::TimedOut;
        result.exit_code = -1;
        spdlog::error("[build] process timed out after 10 minutes");
        TerminateProcess(process_handle, 1);
    } else if (wait_result == WAIT_OBJECT_0) {
        DWORD exit_code = 0;
        GetExitCodeProcess(process_handle, &exit_code);
        result.exit_code = static_cast<int>(exit_code);
        result.state = (exit_code == 0) ? BuildState::Success : BuildState::Failed;
        if (exit_code == 0) {
            spdlog::info("[build] completed successfully");
        } else {
            spdlog::error("[build] failed with exit code {}", exit_code);
        }
    } else {
        result.state = BuildState::Failed;
        result.exit_code = -1;
        spdlog::error("[build] WaitForSingleObject failed");
    }

    result.stdout_log = stdout_log;
    result.stderr_log = stderr_log;

    // Close handles.
    if (stdout_read != INVALID_HANDLE_VALUE) {
        CloseHandle(stdout_read);
    }
    if (stderr_read != INVALID_HANDLE_VALUE) {
        CloseHandle(stderr_read);
    }
    if (process_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(process_handle);
    }
}

Result<bool, std::string> BuildRunner::run_build(
    const std::filesystem::path& build_dir,
    const std::string& config,
    const std::string& target) {
    // Kill any existing build first.
    kill_build();

    // Create pipes for stdout/stderr.
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&impl_->stdout_read, &impl_->stdout_write, &sa, 0)) {
        return Result<bool, std::string>::err(
            "Failed to create stdout pipe: " + std::to_string(GetLastError()));
    }
    if (!CreatePipe(&impl_->stderr_read, &impl_->stderr_write, &sa, 0)) {
        CloseHandle(impl_->stdout_read);
        CloseHandle(impl_->stdout_write);
        return Result<bool, std::string>::err(
            "Failed to create stderr pipe: " + std::to_string(GetLastError()));
    }

    // Build command line.
    std::string cmd_line = "cmake --build \"" + build_dir.string() +
        "\" --config " + config + " --target " + target;

    // Set up process startup info.
    STARTUPINFOA startup_info{};
    startup_info.cb = sizeof(STARTUPINFOA);
    startup_info.hStdOutput = impl_->stdout_write;
    startup_info.hStdError = impl_->stderr_write;
    startup_info.dwFlags = STARTF_USESTDHANDLES;

    // Create process.
    if (!CreateProcessA(
            nullptr,                              // lpApplicationName
            const_cast<char*>(cmd_line.c_str()), // lpCommandLine
            nullptr,                              // lpProcessAttributes
            nullptr,                              // lpThreadAttributes
            TRUE,                                 // bInheritHandles
            CREATE_NO_WINDOW,                     // dwCreationFlags
            nullptr,                              // lpEnvironment
            build_dir.string().c_str(),          // lpCurrentDirectory
            &startup_info,                        // lpStartupInfo
            &impl_->pi                            // lpProcessInformation
        )) {
        DWORD err = GetLastError();
        CloseHandle(impl_->stdout_read);
        CloseHandle(impl_->stdout_write);
        CloseHandle(impl_->stderr_read);
        CloseHandle(impl_->stderr_write);
        return Result<bool, std::string>::err(
            "CreateProcessA failed: " + std::to_string(err));
    }

    impl_->process_handle = impl_->pi.hProcess;
    impl_->start_time = std::chrono::steady_clock::now();

    // Update result state.
    {
        std::lock_guard<std::mutex> lock(impl_->result_mutex);
        impl_->result.state = BuildState::Running;
        impl_->result.exit_code = 0;
        impl_->result.stdout_log.clear();
        impl_->result.stderr_log.clear();
    }

    // Close the main thread's handles; the child process owns the write ends.
    CloseHandle(impl_->pi.hThread);

    // Start monitor thread.
    impl_->monitor_thread = std::thread(
        monitor_process_impl,
        impl_->process_handle,
        impl_->stdout_read,
        impl_->stderr_read,
        std::ref(impl_->result_mutex),
        std::ref(impl_->result));

    spdlog::info("[build] started: {}", cmd_line);
    return Result<bool, std::string>::ok(true);
}

bool BuildRunner::is_running() const {
    std::lock_guard<std::mutex> lock(impl_->result_mutex);
    return impl_->result.state == BuildState::Running;
}

BuildResult BuildRunner::get_result() const {
    std::lock_guard<std::mutex> lock(impl_->result_mutex);
    return impl_->result;
}

Result<bool, std::string> BuildRunner::kill_build() {
    {
        std::lock_guard<std::mutex> lock(impl_->result_mutex);
        if (impl_->result.state != BuildState::Running) {
            return Result<bool, std::string>::ok(true);
        }
    }

    if (impl_->process_handle != INVALID_HANDLE_VALUE) {
        if (!TerminateProcess(impl_->process_handle, 1)) {
            return Result<bool, std::string>::err(
                "TerminateProcess failed: " + std::to_string(GetLastError()));
        }
    }

    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }

    return Result<bool, std::string>::ok(true);
}

}  // namespace odyssey::editor
