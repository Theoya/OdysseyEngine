#pragma once

#include "app/game.h"

#include <string>
#include <vector>
#include <random>

namespace odyssey {

// Gameplay state visible to HUD
struct GameplayState {
    float player_health = 150.0f;
    float player_max_health = 150.0f;
    int   player_ammo = 40;
    int   score = 0;
    int   enemies_alive = 0;
    int   enemies_total = 0;
    bool  game_over = false;
    bool  player_won = false;
    float game_over_timer = 0.0f;
};

class ShooterGame : public Game {
public:
    Result<bool> on_init(GameContext& ctx) override;
    void on_tick(GameContext& ctx) override;
    const std::vector<RenderEntity>& get_renderables() const override;
    HUDParams get_hud_params() const override;
    void on_shutdown() override;

private:
    // Archetype-to-renderable mapping
    struct ArchetypeMapping {
        std::string archetype_name;
        uint32_t start_index;
        uint32_t count;
        vec4 color;
    };

    // Per-entity simulation state
    struct EntitySim {
        vec3 spawn_position;
        vec3 velocity{0.f};
        float phase;
        float health = -1.0f;
        float max_health = 0.f;
        float attack_cooldown = 0.f;
        float death_timer = 0.f;
        bool alive = true;
        bool is_enemy = false;
    };

    // CPU-side projectile
    struct Projectile {
        vec3 position;
        vec3 velocity;
        float lifetime;
        uint32_t render_index;
        bool from_enemy = false;
    };

    // Rendering
    std::vector<RenderEntity> renderables_;
    std::vector<EntitySim> entity_sims_;
    std::vector<ArchetypeMapping> archetype_mappings_;
    std::vector<Projectile> projectiles_;

    // Player state
    vec3 player_position_{0.f, 1.f, 0.f};
    vec3 camera_position_{0.f, 2.f, -5.f};

    // Timers
    float elapsed_time_ = 0.f;
    float shoot_cooldown_ = 0.f;
    float ammo_regen_timer_ = 0.f;
    float muzzle_flash_ = 0.f;
    float damage_flash_ = 0.f;
    float prev_health_ = 150.f;
    float grace_period_ = 3.0f;

    GameplayState gameplay_;

    // Scene path for restart
    std::filesystem::path scene_path_;

    // Helpers
    void shoot(const vec3& origin, const vec3& direction);
    void tick_projectiles(float dt);
    void tick_enemies(float dt);
    void check_collisions();
    void spawn_enemy_projectile(const vec3& origin, const vec3& direction);
    bool is_enemy_archetype(const std::string& name) const;
    static vec4 archetype_color(const std::string& name);
};

} // namespace odyssey
