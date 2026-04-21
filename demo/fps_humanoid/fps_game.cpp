#include "fps_humanoid/fps_game.h"
#include "app/camera.h"
#include "app/input.h"

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cmath>
#include <algorithm>
#include <cstdio>

namespace odyssey {

// ---------------------------------------------------------------------------
// Game-specific constants
// ---------------------------------------------------------------------------
static constexpr float PROJECTILE_SPEED       = 80.0f;
static constexpr float PROJECTILE_LIFETIME    = 3.0f;
static constexpr float SHOOT_COOLDOWN         = 0.15f;
static constexpr float HIT_RADIUS             = 1.5f;
static constexpr float PROJECTILE_DAMAGE      = 25.0f;
static constexpr int   KILL_SCORE             = 100;
static constexpr float ENEMY_ATTACK_RANGE     = 3.5f;
static constexpr float ENEMY_DAMAGE_PER_SEC   = 10.0f;
static constexpr float AMMO_REGEN_INTERVAL    = 2.0f;
static constexpr int   AMMO_REGEN_AMOUNT      = 2;
static constexpr int   MAX_AMMO               = 60;
static constexpr float ENEMY_APPROACH_DIST    = 10.0f;
static constexpr float ENEMY_STOP_DIST        = 8.0f;
static constexpr float ENEMY_MOVE_SPEED       = 4.0f;
static constexpr int   NUM_ENEMIES            = 8;
static constexpr float SPAWN_RADIUS           = 30.0f;
static constexpr float PLAYER_HIT_RADIUS      = 2.0f;

// The Berserk-Halo hero enemy: replaces enemy index 0 and spawns at
// closer range with higher HP — mini-boss encounter.
static constexpr int   BERSERK_ENEMY_INDEX    = 0;
static constexpr float BERSERK_SPAWN_RADIUS   = 22.0f;
static constexpr float BERSERK_HP             = 180.0f;
static const    char*  BERSERK_ASSETS_DIR     = "demo/showcase/assets/berserk_halo_mk3";

// ---------------------------------------------------------------------------
// on_init — spawn enemies in a circle, set up ground plane
// ---------------------------------------------------------------------------
Result<bool> FPSHumanoidGame::on_init(GameContext& ctx) {
    assets_dir_ = "demo/fps_humanoid/assets";

    // Add ground plane at y=0
    physics::GroundPlane ground;
    ground.point = {0.0f, 0.0f, 0.0f};
    ground.normal = {0.0f, 1.0f, 0.0f};
    collision_system_.add_ground_plane(ground);

    // Initialize enemies in a circle
    enemies_.clear();
    enemies_.resize(NUM_ENEMIES);
    gameplay_ = FPSGameplayState{};
    projectiles_.clear();

    for (int i = 0; i < NUM_ENEMIES; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(NUM_ENEMIES)
                      * glm::two_pi<float>();
        // The Berserk-Halo hero spawns at a closer, facing-player position.
        bool is_berserk = (i == BERSERK_ENEMY_INDEX);
        float radius = is_berserk ? BERSERK_SPAWN_RADIUS : SPAWN_RADIUS;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        auto& enemy = enemies_[i];
        enemy.position = {x, 0.0f, z};
        enemy.death_timer = 0.0f;
        enemy.move_speed = 0.0f;
        enemy.was_walking = false;
        enemy.alive = true;
        enemy.is_berserk = is_berserk;

        if (is_berserk) {
            // Mini-boss: more HP, heavier armor color, uses BerserkHaloCharacter.
            enemy.health = BERSERK_HP;
            enemy.max_health = BERSERK_HP;
            enemy.base_color = {0.12f, 0.12f, 0.14f, 1.0f}; // near-black matte

            enemy.berserk_character = std::make_unique<BerserkHaloCharacter>();
            auto init_result = enemy.berserk_character->initialize(
                assets_dir_, std::filesystem::path(BERSERK_ASSETS_DIR));
            if (init_result.is_err()) {
                spdlog::error("FPSHumanoidGame: failed to init BERSERK enemy: {}",
                              init_result.error());
                // Fallback to stock humanoid on failure so the game still runs.
                enemy.berserk_character.reset();
                enemy.is_berserk = false;
                auto fb = enemy.character.initialize(assets_dir_);
                if (fb.is_err()) continue;
                enemy.character.set_color(enemy.base_color);
                enemy.character.play_idle();
            } else {
                enemy.berserk_character->set_base_color(enemy.base_color);
                enemy.berserk_character->play_idle();
            }
        } else {
            enemy.health = 60.0f;
            enemy.max_health = 60.0f;
            enemy.base_color = {0.9f, 0.15f, 0.1f, 1.0f};

            // Vary colors slightly per enemy
            float hue_shift = static_cast<float>(i) * 0.05f;
            enemy.base_color.r = std::clamp(0.9f + hue_shift, 0.0f, 1.0f);
            enemy.base_color.g = std::clamp(0.15f - hue_shift * 0.3f, 0.0f, 1.0f);

            auto init_result = enemy.character.initialize(assets_dir_);
            if (init_result.is_err()) {
                spdlog::warn("FPSHumanoidGame: failed to init enemy {}: {}", i, init_result.error());
                continue;
            }

            enemy.character.set_color(enemy.base_color);
            enemy.character.set_gun_visible(true);
            enemy.character.play_idle();
        }

        gameplay_.enemies_total++;
        gameplay_.enemies_alive++;
    }

    // Reset player state
    gameplay_.player_health = gameplay_.player_max_health;
    gameplay_.player_ammo = 40;
    gameplay_.score = 0;
    gameplay_.game_over = false;
    gameplay_.player_won = false;
    gameplay_.game_over_timer = 0.0f;

    elapsed_time_ = 0.0f;
    shoot_cooldown_ = 0.0f;
    muzzle_flash_ = 0.0f;
    damage_flash_ = 0.0f;
    ammo_regen_timer_ = 0.0f;
    prev_health_ = gameplay_.player_max_health;
    grace_period_ = 3.0f;

    // Build initial renderables
    rebuild_renderables();

    spdlog::info("FPSHumanoidGame: initialized with {} humanoid enemies", gameplay_.enemies_total);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// on_tick — main game loop
// ---------------------------------------------------------------------------
void FPSHumanoidGame::on_tick(GameContext& ctx) {
    float dt = ctx.delta_time;
    elapsed_time_ += dt;
    shoot_cooldown_ = std::max(0.0f, shoot_cooldown_ - dt);
    muzzle_flash_ = std::max(0.0f, muzzle_flash_ - dt);
    damage_flash_ = std::max(0.0f, damage_flash_ - dt);

    camera_position_ = ctx.camera->position();

    // Game over handling
    if (gameplay_.game_over) {
        gameplay_.game_over_timer += dt;
        if (ctx.input->is_key_down(GLFW_KEY_R)) {
            projectiles_.clear();
            enemies_.clear();
            on_init(ctx);
        }
        return;
    }

    // Input: shooting (left mouse while cursor captured)
    if (ctx.input->is_cursor_captured() &&
        ctx.input->is_mouse_button_down(0)) {
        shoot(ctx.camera->position(), ctx.camera->front());
    }

    // Simulation
    tick_projectiles(dt);
    tick_enemies(dt);
    check_collisions();

    // Grace period
    if (grace_period_ > 0.0f) {
        grace_period_ -= dt;
    }

    // Health tracking / damage flash
    gameplay_.player_health = std::max(0.0f, gameplay_.player_health);
    if (gameplay_.player_health < prev_health_) {
        damage_flash_ = 0.3f;
    }
    prev_health_ = gameplay_.player_health;

    // Player death
    if (gameplay_.player_health <= 0.0f) {
        gameplay_.game_over = true;
        gameplay_.player_won = false;
        spdlog::info("GAME OVER! Final score: {}", gameplay_.score);
    }

    // Health regen (slow)
    if (gameplay_.player_health > 0.0f && gameplay_.player_health < gameplay_.player_max_health) {
        gameplay_.player_health = std::min(
            gameplay_.player_health + 2.0f * dt, gameplay_.player_max_health);
    }

    // Ammo regen
    ammo_regen_timer_ += dt;
    if (ammo_regen_timer_ >= AMMO_REGEN_INTERVAL) {
        ammo_regen_timer_ -= AMMO_REGEN_INTERVAL;
        gameplay_.player_ammo = std::min(gameplay_.player_ammo + AMMO_REGEN_AMOUNT, MAX_AMMO);
    }

    // Win condition
    if (gameplay_.enemies_alive <= 0 && gameplay_.enemies_total > 0) {
        gameplay_.game_over = true;
        gameplay_.player_won = true;
        spdlog::info("VICTORY! All enemies defeated. Score: {}", gameplay_.score);
    }

    // Rebuild renderables at end of frame
    rebuild_renderables();
}

// ---------------------------------------------------------------------------
// shoot — fire a projectile from the player
// ---------------------------------------------------------------------------
void FPSHumanoidGame::shoot(const vec3& origin, const vec3& direction) {
    if (shoot_cooldown_ > 0.0f) return;
    if (gameplay_.player_ammo <= 0) return;
    if (gameplay_.game_over) return;

    gameplay_.player_ammo--;
    shoot_cooldown_ = SHOOT_COOLDOWN;
    muzzle_flash_ = 0.15f;

    Projectile proj{};
    proj.position = origin + direction * 1.5f;
    proj.velocity = direction * PROJECTILE_SPEED;
    proj.lifetime = PROJECTILE_LIFETIME;
    proj.from_enemy = false;
    projectiles_.push_back(proj);
}

// ---------------------------------------------------------------------------
// tick_projectiles — move projectiles, remove expired ones
// ---------------------------------------------------------------------------
void FPSHumanoidGame::tick_projectiles(float dt) {
    for (auto it = projectiles_.begin(); it != projectiles_.end(); ) {
        it->lifetime -= dt;
        it->position += it->velocity * dt;

        if (it->lifetime <= 0.0f ||
            std::abs(it->position.x) > 200.0f ||
            std::abs(it->position.z) > 200.0f ||
            it->position.y < -10.0f) {
            it = projectiles_.erase(it);
        } else {
            ++it;
        }
    }
}

// ---------------------------------------------------------------------------
// tick_enemies — simple AI: face player, approach, attack
// ---------------------------------------------------------------------------
void FPSHumanoidGame::tick_enemies(float dt) {
    for (auto& enemy : enemies_) {
        if (!enemy.alive) {
            // Death animation
            if (enemy.death_timer > 0.0f) {
                enemy.death_timer -= dt;
            }
            continue;
        }

        // Direction to player
        vec3 to_player = camera_position_ - enemy.position;
        to_player.y = 0.0f; // stay on ground plane
        float dist = glm::length(to_player);

        // Face the player
        if (dist > 0.1f) {
            vec3 dir = to_player / dist;
            float target_yaw = std::atan2(dir.x, dir.z);
            enemy.rotation = glm::angleAxis(target_yaw, vec3(0.0f, 1.0f, 0.0f));
        }

        // Movement: approach if far, stop if close
        bool is_moving = false;
        if (dist > ENEMY_APPROACH_DIST) {
            vec3 dir = to_player / dist;
            enemy.position += dir * ENEMY_MOVE_SPEED * dt;
            enemy.move_speed = ENEMY_MOVE_SPEED;
            is_moving = true;
        } else if (dist > ENEMY_STOP_DIST) {
            vec3 dir = to_player / dist;
            float approach_speed = ENEMY_MOVE_SPEED * 0.5f;
            enemy.position += dir * approach_speed * dt;
            enemy.move_speed = approach_speed;
            is_moving = true;
        } else {
            enemy.move_speed = 0.0f;
        }

        // Animation crossfade based on movement state
        if (is_moving && !enemy.was_walking) {
            if (enemy.is_berserk && enemy.berserk_character) {
                enemy.berserk_character->crossfade_to_walk(0.3f);
            } else {
                enemy.character.crossfade_to_walk(0.2f);
            }
            enemy.was_walking = true;
        } else if (!is_moving && enemy.was_walking) {
            if (enemy.is_berserk && enemy.berserk_character) {
                enemy.berserk_character->crossfade_to_idle(0.3f);
            } else {
                enemy.character.crossfade_to_idle(0.2f);
            }
            enemy.was_walking = false;
        }

        // Clamp to ground
        float ground_y = collision_system_.ground_height_at(enemy.position.x, enemy.position.z);
        enemy.position.y = ground_y;

        // Update character (animation + renderables)
        if (enemy.is_berserk && enemy.berserk_character) {
            enemy.berserk_character->update(dt, enemy.position, enemy.rotation,
                                            enemy.move_speed, 0.0f);
            // Hit flash on berserk: red pulse scaled by missing health.
            float health_ratio = enemy.health / enemy.max_health;
            float missing = 1.0f - health_ratio;
            float pulse = std::sin(elapsed_time_ * 10.0f) * 0.5f + 0.5f;
            enemy.berserk_character->set_hit_flash(missing * pulse * 0.6f);
        } else {
            enemy.character.update(dt, enemy.position, enemy.rotation,
                                   enemy.move_speed, 0.0f);
        }

        // Proximity melee damage to player
        if (grace_period_ <= 0.0f && dist < ENEMY_ATTACK_RANGE) {
            gameplay_.player_health -= ENEMY_DAMAGE_PER_SEC * dt;
        }

        // Hit flash (stock humanoid): tint red when damaged
        if (!enemy.is_berserk && enemy.health < enemy.max_health) {
            float flash = std::sin(elapsed_time_ * 10.0f) * 0.5f + 0.5f;
            float health_ratio = enemy.health / enemy.max_health;
            vec4 flash_color = glm::mix(vec4(1.0f, 0.0f, 0.0f, 1.0f),
                                        enemy.base_color,
                                        std::max(health_ratio, flash * 0.5f));
            enemy.character.set_color(flash_color);
        }
    }
}

// ---------------------------------------------------------------------------
// check_collisions — player projectiles vs enemies, enemy proximity
// ---------------------------------------------------------------------------
void FPSHumanoidGame::check_collisions() {
    for (auto proj_it = projectiles_.begin(); proj_it != projectiles_.end(); ) {
        bool hit = false;

        if (proj_it->from_enemy) {
            // Enemy projectile vs player
            if (grace_period_ <= 0.0f) {
                float dist = glm::length(proj_it->position - camera_position_);
                if (dist < PLAYER_HIT_RADIUS) {
                    gameplay_.player_health -= PROJECTILE_DAMAGE;
                    gameplay_.player_health = std::max(0.0f, gameplay_.player_health);
                    hit = true;
                }
            }
        } else {
            // Player projectile vs enemies
            for (auto& enemy : enemies_) {
                if (!enemy.alive) continue;

                float dist = glm::length(proj_it->position - enemy.position);
                if (dist < HIT_RADIUS) {
                    enemy.health -= PROJECTILE_DAMAGE;

                    if (enemy.health <= 0.0f) {
                        enemy.alive = false;
                        enemy.death_timer = 0.5f;
                        gameplay_.enemies_alive--;
                        gameplay_.score += KILL_SCORE;

                        spdlog::info("Enemy killed! Score: {} | Remaining: {}",
                                     gameplay_.score, gameplay_.enemies_alive);
                    }

                    hit = true;
                    break;
                }
            }
        }

        if (hit) {
            proj_it = projectiles_.erase(proj_it);
        } else {
            ++proj_it;
        }
    }
}

// ---------------------------------------------------------------------------
// rebuild_renderables — assemble all visible entities for this frame
// ---------------------------------------------------------------------------
void FPSHumanoidGame::rebuild_renderables() {
    all_renderables_.clear();

    // Ground plane
    RenderEntity ground{};
    ground.position = {0.0f, 0.0f, 0.0f};
    ground.color = {0.15f, 0.35f, 0.1f, 1.0f};
    ground.scale = vec3(1.0f);
    ground.mesh_type = 2; // ground
    all_renderables_.push_back(ground);

    // Enemies (humanoid stick figures)
    for (auto& enemy : enemies_) {
        if (!enemy.alive) {
            // Death effect: shrinking sphere
            if (enemy.death_timer > 0.0f) {
                float t = enemy.death_timer / 0.5f;
                RenderEntity death_sphere{};
                death_sphere.position = enemy.position;
                death_sphere.position.y += 1.0f;
                death_sphere.color = {1.0f, 1.0f, 1.0f, t};
                death_sphere.scale = vec3(t * 1.5f);
                death_sphere.mesh_type = 1; // sphere
                all_renderables_.push_back(death_sphere);
            }
            continue;
        }

        // Append character renderables: berserk armor pieces OR stock stick-figure.
        if (enemy.is_berserk && enemy.berserk_character) {
            const auto& bh = enemy.berserk_character->get_renderables();
            all_renderables_.insert(all_renderables_.end(), bh.begin(), bh.end());
        } else {
            const auto& char_renderables = enemy.character.get_renderables();
            all_renderables_.insert(all_renderables_.end(),
                                    char_renderables.begin(),
                                    char_renderables.end());
        }
    }

    // Projectiles
    for (const auto& proj : projectiles_) {
        RenderEntity bullet{};
        bullet.position = proj.position;
        bullet.scale = vec3(0.15f);
        bullet.mesh_type = 1; // sphere
        if (proj.from_enemy) {
            bullet.color = {1.0f, 0.3f, 0.1f, 1.0f}; // red
        } else {
            bullet.color = {1.0f, 1.0f, 0.2f, 1.0f}; // yellow
        }
        all_renderables_.push_back(bullet);
    }

    // Player weapon view: floating gun in front of camera
    {
        vec3 gun_pos = camera_position_
                       + vec3(0.3f, -0.3f, 0.0f); // offset right and down
        // Simple gun box (no skeleton for first-person view)
        RenderEntity gun{};
        gun.position = gun_pos;
        gun.color = {0.3f, 0.3f, 0.35f, 1.0f};
        gun.scale = {0.05f, 0.05f, 0.3f};
        gun.mesh_type = 0; // box
        all_renderables_.push_back(gun);
    }

    // Crosshair dot at center of view (small bright sphere)
    {
        RenderEntity crosshair{};
        crosshair.position = camera_position_;
        crosshair.color = {1.0f, 1.0f, 1.0f, 0.8f};
        crosshair.scale = vec3(0.01f);
        crosshair.mesh_type = 1; // sphere
        all_renderables_.push_back(crosshair);
    }
}

// ---------------------------------------------------------------------------
// get_renderables
// ---------------------------------------------------------------------------
const std::vector<RenderEntity>& FPSHumanoidGame::get_renderables() const {
    return all_renderables_;
}

// ---------------------------------------------------------------------------
// get_hud_params — health bar, window title, post-process effects
// ---------------------------------------------------------------------------
HUDParams FPSHumanoidGame::get_hud_params() const {
    HUDParams hud{};
    float health_pct = gameplay_.player_max_health > 0.0f
        ? gameplay_.player_health / gameplay_.player_max_health : 1.0f;

    hud.health_pct = health_pct;
    hud.alert_level = gameplay_.game_over ? 1.0f
        : (gameplay_.enemies_alive > 0 ? 0.3f : 0.0f);
    hud.sync_ratio = gameplay_.enemies_total > 0
        ? 1.0f - static_cast<float>(gameplay_.enemies_alive) /
                 static_cast<float>(gameplay_.enemies_total)
        : 1.0f;

    // Muzzle flash brightness
    hud.brightness_boost = muzzle_flash_ * 3.0f;

    // Low health distortion
    float danger = 1.0f - health_pct;
    hud.chromatic_boost = danger * 4.0f;
    hud.flicker_boost = danger * 0.6f;

    // Damage flash
    hud.curvature_boost = damage_flash_ * 8.0f;
    hud.vignette_boost = damage_flash_ * 2.0f;

    // Window title HUD
    char title[256];
    if (gameplay_.game_over) {
        if (gameplay_.player_won) {
            std::snprintf(title, sizeof(title),
                "OdysseyEngine | VICTORY! Score: %d | Press R to restart",
                gameplay_.score);
        } else {
            std::snprintf(title, sizeof(title),
                "OdysseyEngine | GAME OVER | Score: %d | Press R to restart",
                gameplay_.score);
        }
    } else {
        std::snprintf(title, sizeof(title),
            "OdysseyEngine | HP: %.0f | Ammo: %d | Score: %d | Enemies: %d/%d",
            gameplay_.player_health, gameplay_.player_ammo, gameplay_.score,
            gameplay_.enemies_alive, gameplay_.enemies_total);
    }
    hud.window_title = title;

    return hud;
}

// ---------------------------------------------------------------------------
// on_shutdown
// ---------------------------------------------------------------------------
void FPSHumanoidGame::on_shutdown() {
    enemies_.clear();
    projectiles_.clear();
    all_renderables_.clear();
}

// ---------------------------------------------------------------------------
// create_game — factory function the engine calls
// ---------------------------------------------------------------------------
std::unique_ptr<Game> create_game() {
    return std::make_unique<FPSHumanoidGame>();
}

} // namespace odyssey
