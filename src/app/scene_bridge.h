#pragma once

#include "core/types.h"
#include "core/result.h"

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

namespace odyssey {

// Forward declarations
namespace scene { class EntityManager; struct SceneData; }
namespace nadir { class NadirSystem; }

// Renderable entity data for the renderer
struct RenderEntity {
    vec3 position;
    vec4 color;         // RGBA
    float scale;        // uniform scale
    uint32_t mesh_type; // 0=box, 1=sphere, 2=ground
};

// Gameplay state visible to the engine
struct GameplayState {
    float player_health = 150.0f;
    float player_max_health = 150.0f;
    int   player_ammo = 40;
    int   score = 0;
    int   enemies_alive = 0;
    int   enemies_total = 0;
    bool  game_over = false;
    bool  player_won = false;
    float game_over_timer = 0.0f;  // seconds since game ended
};

class SceneBridge {
public:
    // Load a scene XML file and set up entities + Nadir archetypes
    Result<bool> load_scene(
        const std::filesystem::path& scene_path,
        scene::EntityManager& entity_mgr,
        nadir::NadirSystem& nadir_sys
    );

    // Called each frame: update entity positions from Nadir output
    void tick(float delta_time, nadir::NadirSystem& nadir_sys);

    // Gameplay: set camera position each frame (for enemy targeting)
    void set_camera_position(const vec3& pos) { camera_position_ = pos; }

    // Gameplay: attempt to fire a projectile from the given position/direction
    void shoot(const vec3& origin, const vec3& direction);

    // Gameplay: restart the game (respawn enemies, reset health/score)
    void restart(scene::EntityManager& entity_mgr, nadir::NadirSystem& nadir_sys,
                 const std::filesystem::path& scene_path);

    // Get renderable entities for the renderer to draw
    const std::vector<RenderEntity>& get_renderables() const { return renderables_; }

    // Get player position (for camera tracking)
    vec3 get_player_position() const { return player_position_; }

    // Get current gameplay state
    const GameplayState& gameplay_state() const { return gameplay_; }

    // Muzzle flash timer (for visual feedback)
    float muzzle_flash() const { return muzzle_flash_; }

    // Damage flash timer (for hit feedback)
    float damage_flash() const { return damage_flash_; }

private:
    struct ArchetypeMapping {
        std::string archetype_name;
        uint32_t start_index;   // index into renderables_
        uint32_t count;
        vec4 color;             // color for this archetype
    };

    // Per-entity simulation state for CPU-side movement
    struct EntitySim {
        vec3 spawn_position;    // where the entity was originally placed
        vec3 velocity{0.f};     // current velocity
        float phase;            // random phase offset for varied motion
        float health = -1.0f;   // -1 = not damageable, 0 = dead
        float max_health = 0.f;
        float attack_cooldown = 0.f;
        float death_timer = 0.f;  // > 0 means dying (shrink + flash animation)
        bool alive = true;
        bool is_enemy = false;
    };

    // CPU-side projectile
    struct Projectile {
        vec3 position;
        vec3 velocity;
        float lifetime;         // seconds remaining
        uint32_t render_index;  // index into renderables_ (if rendered)
        bool from_enemy = false;
    };

    std::vector<RenderEntity> renderables_;
    std::vector<EntitySim> entity_sims_;
    std::vector<ArchetypeMapping> archetype_mappings_;
    std::vector<Projectile> projectiles_;
    vec3 player_position_{0.f, 1.f, 0.f};
    vec3 camera_position_{0.f, 15.f, 30.f};
    float elapsed_time_ = 0.f;
    float shoot_cooldown_ = 0.f;
    float ammo_regen_timer_ = 0.f;
    GameplayState gameplay_;
    float muzzle_flash_ = 0.f;
    float damage_flash_ = 0.f;
    float prev_health_ = 150.f;
    float grace_period_ = 3.0f;  // seconds of invulnerability at start

    // Map archetype names to colors
    static vec4 archetype_color(const std::string& name);

    // Gameplay helpers
    void tick_projectiles(float dt);
    void tick_enemies(float dt);
    void check_collisions();
    void spawn_enemy_projectile(const vec3& origin, const vec3& direction);
    bool is_enemy_archetype(const std::string& name) const;
};

} // namespace odyssey
