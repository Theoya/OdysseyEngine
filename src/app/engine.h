#pragma once

#include "core/types.h"
#include "core/result.h"
#include "core/mode.h"
#include "app/game.h"
#include "physics/physics_world.h"

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
    bool fullscreen = false;
    bool validation_layers = true;
    uint32_t gpu_index = 0;

    // Nadir subsystem
    std::filesystem::path behavior_dir = "behaviors/shaders";
    std::filesystem::path lib_dir      = "behaviors/lib";
    bool hot_reload   = true;
    uint32_t max_agents = 100000;

    // Scene
    std::filesystem::path scene_path;

    // Shaders
    std::filesystem::path shader_dir = "shaders";
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
    /// If a Game is provided, the engine will call its lifecycle methods.
    /// If null, engine runs as a renderer-only (no gameplay).
    Result<bool> initialize(const EngineConfig& config,
                            std::unique_ptr<Game> game = nullptr);

    /// Enter the main loop — blocks until the window is closed.
    void run();

    /// Tear down all subsystems in reverse order.
    void shutdown();

    /// True while the engine loop is active.
    bool is_running() const { return running_; }

    // ---- Phase 2: Mode gating --------------------------------------------
    // The current execution mode. Gates Nadir dispatch, script tick, and
    // physics step. Rendering and camera input remain active in every mode.
    Mode mode() const { return mode_; }

    // Set the current mode. Safe to call at any time — changes take effect
    // on the next process_frame(). Emits a log line for traceability.
    void set_mode(Mode m);

    // Pure predicates exposed for test harnesses. Thin wrappers over
    // core/mode.h that capture the specific per-subsystem behavior the
    // engine's main loop implements.
    bool tick_would_dispatch_nadir()  const { return mode_runs_nadir(mode_); }
    bool tick_would_run_scripts()     const { return mode_runs_scripts(mode_); }
    bool tick_would_step_physics()    const { return mode_runs_physics(mode_); }
    bool tick_would_run_camera()      const { return mode_runs_camera(mode_); }

    // Phase 9: Physics world accessors
    // const getter for all code
    const physics::PhysicsWorld& physics_world() const { return physics_world_; }
    // mutable getter gated to pre_physics script phase only (documented contract)
    physics::PhysicsWorld& physics_world_mut() { return physics_world_; }

private:
    // Per-frame work
    void process_frame(float delta_time);
    void render_entities(const mat4& vp);

    // Subsystem init helpers
    Result<bool> init_window(const EngineConfig& config);
    Result<bool> init_vulkan(const EngineConfig& config);
    Result<bool> init_nadir(const EngineConfig& config);

    // Subsystem shutdown helpers
    void shutdown_nadir();
    void shutdown_vulkan();
    void shutdown_window();

    bool running_ = false;
    Mode mode_ = Mode::Play;
    GLFWwindow* window_ = nullptr;

    // Resize / fullscreen
    bool framebuffer_resized_ = false;
    bool fullscreen_ = false;
    bool f11_was_pressed_ = false;
    int windowed_x_ = 0, windowed_y_ = 0;
    int windowed_w_ = 1920, windowed_h_ = 1080;

    void recreate_swapchain();
    void toggle_fullscreen();

    // Frame tracking
    uint32_t current_frame_ = 0;
    float total_time_   = 0.0f;
    double last_time_   = 0.0;

    // Phase 9: Physics world with fixed-dt accumulator
    physics::PhysicsWorld physics_world_;
    double physics_accumulator_ = 0.0;

    // Vulkan + Nadir state kept behind a pimpl wall so this header
    // does not pull in Vulkan or Nadir headers.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace odyssey
