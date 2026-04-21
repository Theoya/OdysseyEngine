#pragma once

#include "core/types.h"
#include "core/result.h"
#include "core/mode.h"

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

struct GLFWwindow;

namespace odyssey {

class Camera;
class InputManager;
namespace scene { class EntityManager; }
namespace nadir { class NadirSystem; }

// Renderable entity data — games produce these, engine draws them.
struct RenderEntity {
    vec3 position;
    quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    vec4 color;         // RGBA
    vec3 scale{1.0f};   // non-uniform scale
    uint32_t mesh_type; // 0=box, 1=sphere, 2=ground, 3=cylinder
};

// HUD parameters that the game can set each frame.
// These drive the EVA post-processing overlay.
struct HUDParams {
    float health_pct = 1.0f;
    float alert_level = 0.0f;
    float sync_ratio = 0.85f;

    // CRT effect overrides
    float brightness_boost = 0.0f;   // added to base 1.2
    float chromatic_boost = 0.0f;    // added to base 1.0
    float flicker_boost = 0.0f;      // added to base 0.2
    float curvature_boost = 0.0f;    // added to base 2.0
    float vignette_boost = 0.0f;     // added to base 0.8

    // Window title text (displayed as HUD fallback)
    std::string window_title;
};

// Read-only engine context provided to the game each frame.
struct GameContext {
    float delta_time = 0.f;
    float total_time = 0.f;
    Camera* camera = nullptr;
    InputManager* input = nullptr;
    scene::EntityManager* entity_mgr = nullptr;
    nadir::NadirSystem* nadir_sys = nullptr;
    GLFWwindow* window = nullptr;
    std::filesystem::path scene_path;

    // Phase 2: execution mode. Games that implement their own script/physics
    // tick should gate on mode_runs_scripts() / mode_runs_physics() so the
    // editor's Edit/Simulate modes behave correctly.
    Mode mode = Mode::Play;
};

// ---------------------------------------------------------------------------
// Game — abstract interface. Implement this to create a game using the engine.
// ---------------------------------------------------------------------------
class Game {
public:
    virtual ~Game() = default;

    // Called once after engine subsystems are initialized.
    // Load your scene, set up game state, etc.
    virtual Result<bool> on_init(GameContext& ctx) = 0;

    // Called each frame. Update game logic, AI, physics, etc.
    virtual void on_tick(GameContext& ctx) = 0;

    // Return the list of entities to render this frame.
    virtual const std::vector<RenderEntity>& get_renderables() const = 0;

    // Return HUD parameters for post-processing.
    virtual HUDParams get_hud_params() const = 0;

    // Called on engine shutdown.
    virtual void on_shutdown() = 0;
};

// Factory function — games implement this to register themselves.
// The engine calls this to create the game instance.
std::unique_ptr<Game> create_game();

} // namespace odyssey
