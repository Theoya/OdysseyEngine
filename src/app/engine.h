#pragma once

#include "core/types.h"
#include "core/result.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

// Forward-declare GLFW window to avoid header pollution.
struct GLFWwindow;

namespace odyssey {

/// Engine configuration — loaded from engine.xml or constructed in code.
struct EngineConfig {
    uint32_t window_width  = 1920;
    uint32_t window_height = 1080;
    std::string window_title = "OdysseyEngine";
    bool vsync = true;
    bool validation_layers = true;
    uint32_t gpu_index = 0;

    // Nadir subsystem
    std::filesystem::path behavior_dir = "behaviors/shaders";
    std::filesystem::path lib_dir      = "behaviors/lib";
    bool hot_reload   = true;
    uint32_t max_agents = 100000;
};

/// Pure function: parse an engine.xml config file into EngineConfig.
/// Returns an error string if the file is missing or malformed.
Result<EngineConfig> parse_engine_config(const std::filesystem::path& config_path);

// ---------------------------------------------------------------------------
// Engine — owns the GLFW window, Vulkan context, and Nadir system.
// ---------------------------------------------------------------------------
class Engine {
public:
    Engine();
    ~Engine();

    // Non-copyable, non-movable.
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /// Initialize all subsystems (window, Vulkan, Nadir).
    Result<bool> initialize(const EngineConfig& config);

    /// Enter the main loop — blocks until the window is closed.
    void run();

    /// Tear down all subsystems in reverse order.
    void shutdown();

    /// True while the engine loop is active.
    bool is_running() const { return running_; }

private:
    // Per-frame work
    void process_frame(float delta_time);

    // Subsystem init helpers
    Result<bool> init_window(const EngineConfig& config);
    Result<bool> init_vulkan(const EngineConfig& config);
    Result<bool> init_nadir(const EngineConfig& config);

    // Subsystem shutdown helpers
    void shutdown_nadir();
    void shutdown_vulkan();
    void shutdown_window();

    bool running_ = false;
    GLFWwindow* window_ = nullptr;

    // Frame tracking
    uint32_t current_frame_ = 0;
    float total_time_   = 0.0f;
    double last_time_   = 0.0;

    // Vulkan + Nadir state kept behind a pimpl wall so this header
    // does not pull in Vulkan or Nadir headers.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace odyssey
