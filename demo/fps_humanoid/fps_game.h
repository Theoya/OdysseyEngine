#pragma once
#include "app/game.h"
#include "fps_humanoid/humanoid_character.h"
#include "physics/collision_system.h"
#include <vector>
#include <string>

namespace odyssey {

struct FPSGameplayState {
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

class FPSHumanoidGame : public Game {
public:
    Result<bool> on_init(GameContext& ctx) override;
    void on_tick(GameContext& ctx) override;
    const std::vector<RenderEntity>& get_renderables() const override;
    HUDParams get_hud_params() const override;
    void on_shutdown() override;

private:
    struct EnemyState {
        HumanoidCharacter character;
        vec3 position;
        quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        float health = 60.0f;
        float max_health = 60.0f;
        bool alive = true;
        float death_timer = 0.0f;
        float move_speed = 0.0f;
        bool was_walking = false;
        vec4 base_color{0.9f, 0.15f, 0.1f, 1.0f};
    };

    struct Projectile {
        vec3 position;
        vec3 velocity;
        float lifetime;
        bool from_enemy = false;
    };

    // Rendering
    std::vector<RenderEntity> all_renderables_;

    // Enemies
    std::vector<EnemyState> enemies_;

    // Projectiles
    std::vector<Projectile> projectiles_;

    // Collision
    physics::CollisionSystem collision_system_;

    // Player state
    FPSGameplayState gameplay_;
    vec3 camera_position_{0.0f};
    float shoot_cooldown_ = 0.0f;
    float muzzle_flash_ = 0.0f;
    float damage_flash_ = 0.0f;
    float elapsed_time_ = 0.0f;
    float ammo_regen_timer_ = 0.0f;
    float prev_health_ = 150.0f;
    float grace_period_ = 3.0f;

    // Assets path
    std::filesystem::path assets_dir_;

    // Helpers
    void shoot(const vec3& origin, const vec3& direction);
    void tick_projectiles(float dt);
    void tick_enemies(float dt);
    void check_collisions();
    void rebuild_renderables();
};

// Factory function
std::unique_ptr<Game> create_game();

} // namespace odyssey
