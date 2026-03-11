#include "shooter_game.h"
#include "app/camera.h"
#include "app/input.h"
#include "scene/scene_loader.h"
#include "scene/entity_manager.h"
#include "nadir/nadir_system.h"
#include "nadir/action_sequence.h"

#include <spdlog/spdlog.h>
#include <glm/gtc/constants.hpp>
#include <GLFW/glfw3.h>

#include <cmath>
#include <algorithm>
#include <cstdio>

namespace odyssey {

// ---------------------------------------------------------------------------
// Game-specific constants — these define the shooter's feel and balance.
// Engine knows nothing about any of this.
// ---------------------------------------------------------------------------
static constexpr float PROJECTILE_SPEED       = 80.0f;
static constexpr float PROJECTILE_LIFETIME    = 3.0f;
static constexpr float SHOOT_COOLDOWN         = 0.15f;
static constexpr float HIT_RADIUS             = 1.5f;
static constexpr float PROJECTILE_DAMAGE      = 25.0f;
static constexpr int   KILL_SCORE             = 100;
static constexpr int   BOSS_KILL_SCORE        = 500;
static constexpr float ENEMY_ATTACK_RANGE     = 3.5f;
static constexpr float ENEMY_DAMAGE_PER_SEC   = 10.0f;
static constexpr float BOSS_DAMAGE_PER_SEC    = 20.0f;
static constexpr float AMMO_REGEN_INTERVAL    = 2.0f;
static constexpr int   AMMO_REGEN_AMOUNT      = 2;
static constexpr int   MAX_AMMO               = 60;
static constexpr float ENEMY_RANGED_COOLDOWN  = 3.5f;
static constexpr float ENEMY_PROJECTILE_SPEED = 20.0f;
static constexpr float ENEMY_PROJECTILE_DAMAGE = 8.0f;
static constexpr float PLAYER_HIT_RADIUS      = 2.0f;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
vec4 ShooterGame::archetype_color(const std::string& name) {
    if (name == "player")              return {0.1f, 0.9f, 0.2f, 1.0f};
    if (name == "enemy_pack_hunter")   return {0.9f, 0.15f, 0.1f, 1.0f};
    if (name == "multi_arm_gunner")    return {0.8f, 0.05f, 0.2f, 1.0f};
    if (name == "enemy_ranged")        return {1.0f, 0.5f, 0.1f, 1.0f};
    if (name == "civilian")            return {0.2f, 0.8f, 0.9f, 1.0f};
    if (name == "projectile")          return {1.0f, 1.0f, 0.2f, 1.0f};
    if (name == "static_cover")        return {0.4f, 0.4f, 0.4f, 1.0f};
    return {1.0f, 1.0f, 1.0f, 1.0f};
}

bool ShooterGame::is_enemy_archetype(const std::string& name) const {
    return name == "enemy_pack_hunter" ||
           name == "multi_arm_gunner" ||
           name == "enemy_ranged";
}

// ---------------------------------------------------------------------------
// on_init — load scene, register archetypes, build renderables
// ---------------------------------------------------------------------------
Result<bool> ShooterGame::on_init(GameContext& ctx) {
    scene_path_ = ctx.scene_path;

    auto scene_result = scene::load_scene_file(ctx.scene_path);
    if (scene_result.is_err()) {
        return Result<bool>::err(scene_result.error());
    }
    const auto& scene_data = scene_result.value();

    spdlog::info("ShooterGame: loading scene '{}' ({} entity descriptors)",
                 scene_data.name, scene_data.entities.size());

    scene::populate_entities(*ctx.entity_mgr, scene_data);

    // Register archetypes with Nadir
    struct ArchetypeInfo { uint32_t total_count = 0; std::string shader_path; };
    std::unordered_map<std::string, ArchetypeInfo> archetype_info;

    for (const auto& desc : scene_data.entities) {
        auto& info = archetype_info[desc.archetype];
        info.total_count += desc.count;
        if (!desc.behavior_shader.empty() && info.shader_path.empty()) {
            info.shader_path = desc.behavior_shader;
        }
    }

    for (const auto& [arch_name, info] : archetype_info) {
        if (!info.shader_path.empty()) {
            std::string shader_stem = std::filesystem::path(info.shader_path).stem().string();
            if (ctx.nadir_sys->get_archetype(arch_name) != nullptr ||
                ctx.nadir_sys->get_archetype(shader_stem) != nullptr) {
                continue;
            }
            auto reg_result = ctx.nadir_sys->register_archetype(
                arch_name, info.total_count, info.shader_path);
            if (reg_result.is_err()) {
                spdlog::warn("ShooterGame: failed to register archetype '{}': {}",
                             arch_name, reg_result.error());
            }
        }
    }

    // Build renderables
    renderables_.clear();
    entity_sims_.clear();
    archetype_mappings_.clear();
    projectiles_.clear();
    gameplay_ = GameplayState{};

    // Ground plane
    {
        RenderEntity ground{};
        ground.position = {0.f, 0.f, 0.f};
        ground.color = {0.15f, 0.35f, 0.1f, 1.0f};
        ground.scale = vec3(1.0f);
        ground.mesh_type = 2;
        renderables_.push_back(ground);

        EntitySim dummy{};
        dummy.spawn_position = ground.position;
        dummy.phase = 0.f;
        entity_sims_.push_back(dummy);
    }

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> phase_dist(0.f, glm::two_pi<float>());

    for (const auto& group : ctx.entity_mgr->get_archetype_groups()) {
        const std::string& arch_name = group.archetype_name;
        vec4 color = archetype_color(arch_name);
        uint32_t mesh_type = (arch_name == "static_cover") ? 0u : 1u;

        ArchetypeMapping mapping{};
        mapping.archetype_name = arch_name;
        mapping.nadir_name = arch_name;
        mapping.start_index = static_cast<uint32_t>(renderables_.size());
        mapping.count = static_cast<uint32_t>(group.entity_ids.size());
        mapping.color = color;

        // Resolve NadirSystem archetype name (scene name may differ from shader stem)
        if (ctx.nadir_sys && !ctx.nadir_sys->get_archetype(arch_name)) {
            auto info_it = archetype_info.find(arch_name);
            if (info_it != archetype_info.end() && !info_it->second.shader_path.empty()) {
                std::string stem =
                    std::filesystem::path(info_it->second.shader_path).stem().string();
                if (ctx.nadir_sys->get_archetype(stem)) {
                    mapping.nadir_name = stem;
                }
            }
        }

        float entity_health = -1.0f;
        bool enemy = is_enemy_archetype(arch_name);
        if (arch_name == "enemy_pack_hunter")  entity_health = 60.0f;
        if (arch_name == "enemy_ranged")       entity_health = 40.0f;
        if (arch_name == "multi_arm_gunner")   entity_health = 500.0f;

        for (EntityID eid : group.entity_ids) {
            const auto* entity = ctx.entity_mgr->get_entity(eid);
            if (!entity) continue;

            const vec3& pos = entity->components.transform.position;
            float uniform_scale = entity->components.transform.scale.x;

            RenderEntity re{};
            re.position = pos;
            re.color = color;
            re.scale = vec3(uniform_scale);
            re.mesh_type = mesh_type;
            renderables_.push_back(re);

            EntitySim sim{};
            sim.spawn_position = pos;
            sim.phase = phase_dist(rng);
            sim.health = entity_health;
            sim.max_health = entity_health;
            sim.alive = true;
            sim.is_enemy = enemy;
            sim.attack_cooldown = 2.0f + phase_dist(rng) * 0.5f;
            sim.entity_id = eid;
            entity_sims_.push_back(sim);

            if (enemy) {
                gameplay_.enemies_total++;
                gameplay_.enemies_alive++;
            }
        }

        archetype_mappings_.push_back(mapping);
    }

    // Set actual entity counts in NadirSystem (buffers were allocated for 1024)
    for (const auto& mapping : archetype_mappings_) {
        if (ctx.nadir_sys) {
            ctx.nadir_sys->set_entity_count(mapping.nadir_name, mapping.count);
        }
    }

    // Load action sequences
    action_system_ = nadir::ActionSystem{};
    const std::filesystem::path actions_dir = "demo/actions";
    for (const auto& mapping : archetype_mappings_) {
        std::filesystem::path action_path =
            actions_dir / (mapping.archetype_name + ".actions.xml");
        if (std::filesystem::exists(action_path)) {
            auto result = nadir::parse_action_set(action_path);
            if (result.is_ok()) {
                action_system_.register_action_set(mapping.archetype_name,
                                                    std::move(result.value()));
            }
        }
    }

    gpu_pipeline_ready_ = true;
    frame_number_ = 0;

    spdlog::info("ShooterGame: {} renderables, {} enemies", renderables_.size(), gameplay_.enemies_total);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// on_tick — all gameplay logic lives here, not in the engine
// ---------------------------------------------------------------------------
void ShooterGame::on_tick(GameContext& ctx) {
    float dt = ctx.delta_time;
    elapsed_time_ += dt;
    shoot_cooldown_ = std::max(0.f, shoot_cooldown_ - dt);
    muzzle_flash_ = std::max(0.f, muzzle_flash_ - dt);
    damage_flash_ = std::max(0.f, damage_flash_ - dt);

    camera_position_ = ctx.camera->position();

    // Readback GPU behavior outputs
    if (gpu_pipeline_ready_ && ctx.nadir_sys) {
        readback_gpu_outputs(ctx);
    }

    // Game over handling
    if (gameplay_.game_over) {
        gameplay_.game_over_timer += dt;
        // Restart on R
        if (ctx.input->is_key_down(GLFW_KEY_R)) {
            elapsed_time_ = 0.f;
            shoot_cooldown_ = 0.f;
            grace_period_ = 3.0f;
            prev_health_ = gameplay_.player_max_health;
            projectiles_.clear();
            action_system_.cancel_all();
            gpu_outputs_.clear();
            on_init(ctx);
        }
        return;
    }

    // Input: shooting
    if (ctx.input->is_cursor_captured() &&
        ctx.input->is_mouse_button_down(0)) {
        shoot(ctx.camera->position(), ctx.camera->front());
    }

    // Simulation
    tick_projectiles(dt);
    tick_enemies(dt);
    check_collisions();

    // Action system
    tick_action_system(dt);

    // Grace period
    if (grace_period_ > 0.f) {
        grace_period_ -= dt;
    }

    // Enemy proximity damage
    if (grace_period_ <= 0.f) {
        for (const auto& mapping : archetype_mappings_) {
            if (!is_enemy_archetype(mapping.archetype_name)) continue;
            float dps = (mapping.archetype_name == "multi_arm_gunner")
                ? BOSS_DAMAGE_PER_SEC : ENEMY_DAMAGE_PER_SEC;

            for (uint32_t i = 0; i < mapping.count; ++i) {
                uint32_t idx = mapping.start_index + i;
                if (idx >= entity_sims_.size()) break;
                if (!entity_sims_[idx].alive) continue;

                float dist = glm::length(renderables_[idx].position - camera_position_);
                if (dist < ENEMY_ATTACK_RANGE * renderables_[idx].scale.x) {
                    gameplay_.player_health -= dps * dt;
                }
            }
        }
    }

    // Health
    gameplay_.player_health = std::max(0.f, gameplay_.player_health);
    if (gameplay_.player_health < prev_health_) {
        damage_flash_ = 0.3f;
    }
    prev_health_ = gameplay_.player_health;

    if (gameplay_.player_health <= 0.f) {
        gameplay_.game_over = true;
        gameplay_.player_won = false;
        spdlog::info("GAME OVER! Final score: {}", gameplay_.score);
    }

    // Health regen
    if (gameplay_.player_health > 0.f && gameplay_.player_health < gameplay_.player_max_health) {
        gameplay_.player_health = std::min(
            gameplay_.player_health + 2.0f * dt, gameplay_.player_max_health);
    }

    // Ammo regen
    ammo_regen_timer_ += dt;
    if (ammo_regen_timer_ >= AMMO_REGEN_INTERVAL) {
        ammo_regen_timer_ -= AMMO_REGEN_INTERVAL;
        gameplay_.player_ammo = std::min(gameplay_.player_ammo + AMMO_REGEN_AMOUNT, MAX_AMMO);
    }

    // Win
    if (gameplay_.enemies_alive <= 0 && gameplay_.enemies_total > 0) {
        gameplay_.game_over = true;
        gameplay_.player_won = true;
        spdlog::info("VICTORY! All enemies defeated. Score: {}", gameplay_.score);
    }

    // Upload current state to GPU for next frame's compute
    if (gpu_pipeline_ready_ && ctx.nadir_sys) {
        upload_to_gpu(ctx);
    }
}

// ---------------------------------------------------------------------------
// get_renderables
// ---------------------------------------------------------------------------
const std::vector<RenderEntity>& ShooterGame::get_renderables() const {
    return renderables_;
}

// ---------------------------------------------------------------------------
// get_hud_params — game tells the engine what HUD to display
// ---------------------------------------------------------------------------
HUDParams ShooterGame::get_hud_params() const {
    HUDParams hud{};
    float health_pct = gameplay_.player_max_health > 0.f
        ? gameplay_.player_health / gameplay_.player_max_health : 1.0f;

    hud.health_pct = health_pct;
    hud.alert_level = gameplay_.game_over ? 1.0f
        : (gameplay_.enemies_alive > 0 ? 0.3f : 0.0f);
    hud.sync_ratio = gameplay_.enemies_total > 0
        ? 1.0f - static_cast<float>(gameplay_.enemies_alive) / static_cast<float>(gameplay_.enemies_total)
        : 1.0f;

    // Muzzle flash
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
void ShooterGame::on_shutdown() {
    action_system_.cancel_all();
    gpu_outputs_.clear();
    renderables_.clear();
    entity_sims_.clear();
    archetype_mappings_.clear();
    projectiles_.clear();
}

// ---------------------------------------------------------------------------
// shoot
// ---------------------------------------------------------------------------
void ShooterGame::shoot(const vec3& origin, const vec3& direction) {
    if (shoot_cooldown_ > 0.f) return;
    if (gameplay_.player_ammo <= 0) return;
    if (gameplay_.game_over) return;

    gameplay_.player_ammo--;
    shoot_cooldown_ = SHOOT_COOLDOWN;
    muzzle_flash_ = 0.15f;

    vec3 spawn_pos = origin + direction * 1.5f;

    Projectile proj{};
    proj.position = spawn_pos;
    proj.velocity = direction * PROJECTILE_SPEED;
    proj.lifetime = PROJECTILE_LIFETIME;

    RenderEntity re{};
    re.position = spawn_pos;
    re.color = {1.0f, 1.0f, 0.2f, 1.0f};
    re.scale = vec3(0.15f);
    re.mesh_type = 1;
    proj.render_index = static_cast<uint32_t>(renderables_.size());
    renderables_.push_back(re);

    EntitySim sim{};
    sim.spawn_position = spawn_pos;
    sim.phase = 0.f;
    entity_sims_.push_back(sim);

    projectiles_.push_back(proj);
}

// ---------------------------------------------------------------------------
// spawn_enemy_projectile
// ---------------------------------------------------------------------------
void ShooterGame::spawn_enemy_projectile(const vec3& origin, const vec3& direction) {
    Projectile proj{};
    proj.position = origin;
    proj.velocity = direction * ENEMY_PROJECTILE_SPEED;
    proj.lifetime = PROJECTILE_LIFETIME;
    proj.from_enemy = true;

    RenderEntity re{};
    re.position = origin;
    re.color = {1.0f, 0.3f, 0.1f, 1.0f};
    re.scale = vec3(0.12f);
    re.mesh_type = 1;
    proj.render_index = static_cast<uint32_t>(renderables_.size());
    renderables_.push_back(re);

    EntitySim sim{};
    sim.spawn_position = origin;
    sim.phase = 0.f;
    entity_sims_.push_back(sim);

    projectiles_.push_back(proj);
}

// ---------------------------------------------------------------------------
// tick_projectiles
// ---------------------------------------------------------------------------
void ShooterGame::tick_projectiles(float dt) {
    for (auto it = projectiles_.begin(); it != projectiles_.end(); ) {
        it->lifetime -= dt;
        it->position += it->velocity * dt;

        if (it->render_index < renderables_.size()) {
            renderables_[it->render_index].position = it->position;
        }

        if (it->lifetime <= 0.f ||
            std::abs(it->position.x) > 200.f ||
            std::abs(it->position.z) > 200.f ||
            it->position.y < -10.f) {
            if (it->render_index < renderables_.size()) {
                renderables_[it->render_index].position = {0.f, -100.f, 0.f};
                renderables_[it->render_index].scale = vec3(0.f);
            }
            it = projectiles_.erase(it);
        } else {
            ++it;
        }
    }
}

// ---------------------------------------------------------------------------
// tick_enemies — GPU-driven movement + attack via BehaviorOutput readback
// ---------------------------------------------------------------------------
void ShooterGame::tick_enemies(float dt) {
    for (const auto& mapping : archetype_mappings_) {
        const std::string& arch = mapping.archetype_name;

        // Look up GPU outputs for this archetype (may be empty on first frames)
        const std::vector<BehaviorOutput>* outputs = nullptr;
        auto out_it = gpu_outputs_.find(arch);
        if (out_it != gpu_outputs_.end() && !out_it->second.empty()) {
            outputs = &out_it->second;
        }

        for (uint32_t i = 0; i < mapping.count; ++i) {
            uint32_t idx = mapping.start_index + i;
            if (idx >= renderables_.size()) break;

            RenderEntity& re = renderables_[idx];
            EntitySim& sim = entity_sims_[idx];

            // Death animation (CPU-side, kept as-is)
            if (!sim.alive) {
                if (sim.death_timer > 0.f) {
                    sim.death_timer -= dt;
                    float t = sim.death_timer / 0.5f;
                    re.scale = vec3(t * 1.5f);
                    re.color = {1.f, 1.f, 1.f, t};
                    re.position.y += dt * 3.0f;
                } else {
                    re.position = {0.f, -100.f, 0.f};
                    re.scale = vec3(0.f);
                }
                continue;
            }

            if (arch == "player") {
                re.position = sim.spawn_position;
                re.position.y = sim.spawn_position.y + 0.15f * std::sin(elapsed_time_ * 2.0f);
                player_position_ = re.position;
            }
            else if (is_enemy_archetype(arch) || arch == "civilian") {
                // GPU-driven movement
                if (outputs && i < outputs->size()) {
                    const BehaviorOutput& out = (*outputs)[i];
                    vec3 move = vec3(out.move_vector);
                    float weight = out.move_vector.w;

                    if (weight > 0.01f) {
                        re.position += move * dt * weight;
                        re.position.y = sim.spawn_position.y; // ground clamp
                    }

                    // GPU-driven attack
                    if (sim.is_enemy) {
                        float attack_weight = out.attack_target.w;
                        if (attack_weight > 0.5f) {
                            vec3 target = vec3(out.attack_target);
                            vec3 diff = target - re.position;
                            float len = glm::length(diff);
                            if (len > 0.1f) {
                                vec3 dir = diff / len;
                                float offset = (arch == "multi_arm_gunner") ? 2.0f : 1.5f;
                                spawn_enemy_projectile(re.position + dir * offset, dir);
                            }
                        }

                        // Feed action_request to ActionSystem
                        if (out.action_request != 0) {
                            nadir::ActionRequest req;
                            req.entity = sim.entity_id;
                            req.sequence_id = out.action_request;
                            req.priority = out.action_priority;
                            action_system_.queue_action_request(req, arch);
                        }
                    }
                } else {
                    // Fallback: no GPU output yet, keep at spawn position
                    re.position = sim.spawn_position;
                }

                // Health flash coloring (CPU-side, kept as-is)
                if (sim.is_enemy && sim.health >= 0 && sim.health < sim.max_health) {
                    float flash_anim = std::sin(elapsed_time_ * 10.f) * 0.5f + 0.5f;
                    float health_ratio = sim.health / sim.max_health;
                    re.color = glm::mix(vec4(1.f, 0.f, 0.f, 1.f),
                                        archetype_color(arch),
                                        std::max(health_ratio, flash_anim * 0.5f));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// check_collisions
// ---------------------------------------------------------------------------
void ShooterGame::check_collisions() {
    for (auto proj_it = projectiles_.begin(); proj_it != projectiles_.end(); ) {
        bool hit = false;

        if (proj_it->from_enemy) {
            if (grace_period_ <= 0.f) {
                float dist = glm::length(proj_it->position - camera_position_);
                if (dist < PLAYER_HIT_RADIUS) {
                    gameplay_.player_health -= ENEMY_PROJECTILE_DAMAGE;
                    gameplay_.player_health = std::max(0.f, gameplay_.player_health);
                    hit = true;
                }
            }
        } else {
            for (const auto& mapping : archetype_mappings_) {
                if (!is_enemy_archetype(mapping.archetype_name)) continue;

                for (uint32_t i = 0; i < mapping.count; ++i) {
                    uint32_t idx = mapping.start_index + i;
                    if (idx >= entity_sims_.size()) break;

                    EntitySim& sim = entity_sims_[idx];
                    if (!sim.alive || sim.health <= 0.f) continue;

                    const RenderEntity& re = renderables_[idx];
                    float dist = glm::length(proj_it->position - re.position);
                    float collision_radius = HIT_RADIUS * re.scale.x;

                    if (dist < collision_radius) {
                        sim.health -= PROJECTILE_DAMAGE;

                        if (sim.health <= 0.f) {
                            sim.alive = false;
                            sim.death_timer = 0.5f;
                            gameplay_.enemies_alive--;

                            if (mapping.archetype_name == "multi_arm_gunner") {
                                gameplay_.score += BOSS_KILL_SCORE;
                            } else {
                                gameplay_.score += KILL_SCORE;
                            }

                            spdlog::info("Enemy killed! ({}) Score: {} | Remaining: {}",
                                         mapping.archetype_name, gameplay_.score,
                                         gameplay_.enemies_alive);
                        }

                        hit = true;
                        break;
                    }
                }
                if (hit) break;
            }
        }

        if (hit) {
            if (proj_it->render_index < renderables_.size()) {
                renderables_[proj_it->render_index].position = {0.f, -100.f, 0.f};
                renderables_[proj_it->render_index].scale = vec3(0.f);
            }
            proj_it = projectiles_.erase(proj_it);
        } else {
            ++proj_it;
        }
    }
}

// ---------------------------------------------------------------------------
// readback_gpu_outputs — read BehaviorOutput buffers from GPU
// ---------------------------------------------------------------------------
void ShooterGame::readback_gpu_outputs(GameContext& ctx) {
    gpu_outputs_.clear();
    for (const auto& mapping : archetype_mappings_) {
        if (!is_enemy_archetype(mapping.archetype_name) &&
            mapping.archetype_name != "civilian") continue;
        if (mapping.nadir_name.empty()) continue;

        auto result = ctx.nadir_sys->readback_outputs(mapping.nadir_name);
        if (result.is_ok()) {
            gpu_outputs_[mapping.archetype_name] = std::move(result.value());
        }
    }
}

// ---------------------------------------------------------------------------
// upload_to_gpu — send current positions/stats/world_state to GPU
// ---------------------------------------------------------------------------
void ShooterGame::upload_to_gpu(GameContext& ctx) {
    for (const auto& mapping : archetype_mappings_) {
        // Only upload archetypes that have behavior shaders in NadirSystem
        if (mapping.nadir_name.empty()) continue;
        if (!ctx.nadir_sys->get_archetype(mapping.nadir_name)) continue;

        // Build position array (vec4 per entity)
        std::vector<vec4> positions(mapping.count);
        for (uint32_t i = 0; i < mapping.count; ++i) {
            uint32_t idx = mapping.start_index + i;
            positions[i] = vec4(renderables_[idx].position, 1.0f);
        }
        ctx.nadir_sys->upload_transforms(mapping.nadir_name,
                                          positions.data(), mapping.count);

        // Build stats array
        std::vector<EntityStats> stats(mapping.count);
        for (uint32_t i = 0; i < mapping.count; ++i) {
            uint32_t idx = mapping.start_index + i;
            auto& s = stats[i];
            s.health = entity_sims_[idx].health;
            s.max_health = entity_sims_[idx].max_health;
            s.speed = 5.0f;
            s.ammo = 0.0f;
            s.stamina = 100.0f;
        }
        ctx.nadir_sys->upload_stats(mapping.nadir_name,
                                     stats.data(), mapping.count);
    }

    // Upload world state to all archetypes
    ctx.nadir_sys->upload_world_state_all(
        elapsed_time_, ctx.delta_time, camera_position_, frame_number_++);
}

// ---------------------------------------------------------------------------
// tick_action_system — advance action sequences, consume outputs
// ---------------------------------------------------------------------------
void ShooterGame::tick_action_system(float dt) {
    action_system_.tick(dt);

    // Consume destroy requests
    auto destroy_list = action_system_.consume_destroy_requests();
    for (EntityID eid : destroy_list) {
        // Find the entity and mark as dead
        for (size_t i = 0; i < entity_sims_.size(); ++i) {
            if (entity_sims_[i].entity_id == eid && entity_sims_[i].alive) {
                entity_sims_[i].alive = false;
                entity_sims_[i].death_timer = 0.5f;
                if (entity_sims_[i].is_enemy) {
                    gameplay_.enemies_alive--;
                    gameplay_.score += KILL_SCORE;
                }
                break;
            }
        }
    }

    // Consume persist writes (logged for now, full writeback in future)
    auto persist_writes = action_system_.consume_persist_writes();
    (void)persist_writes;

    // Consume remaining outputs (sound, spawn, signal — logged only)
    auto sounds = action_system_.consume_sound_requests();
    (void)sounds;
    auto spawns = action_system_.consume_spawn_requests();
    (void)spawns;
    auto signals = action_system_.consume_signal_writes();
    (void)signals;
}

// ---------------------------------------------------------------------------
// create_game — factory function the engine calls to create this game
// ---------------------------------------------------------------------------
std::unique_ptr<Game> create_game() {
    return std::make_unique<ShooterGame>();
}

} // namespace odyssey
