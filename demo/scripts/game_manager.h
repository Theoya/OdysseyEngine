#pragma once

#include "scripting/script.h"
#include <string>

namespace odyssey::demo {

// Game states for the shooter demo
enum class GamePhase {
    WAITING,      // waiting for players
    COUNTDOWN,    // 3-2-1 countdown
    PLAYING,      // active gameplay
    ROUND_OVER,   // round ended, showing scores
    GAME_OVER     // all rounds complete
};

// Game manager handles round logic, scoring, and respawns.
// Pure: ScriptContext in -> ScriptResult out. No side effects.
class GameManager : public scripting::Script {
public:
    std::string name() const override { return "GameManager"; }

    scripting::ScriptResult tick(const scripting::ScriptContext& ctx) override {
        scripting::ScriptResult result;

        auto phase = ctx.get<int>("game_phase", static_cast<int>(GamePhase::WAITING));
        float round_timer = ctx.get<float>("round_timer", 0.0f);
        int current_round = ctx.get<int>("current_round", 1);
        int max_rounds = ctx.get<int>("max_rounds", 3);
        int player_score = ctx.get<int>("player_score", 0);

        float dt = ctx.delta_time;
        round_timer += dt;

        switch (static_cast<GamePhase>(phase)) {
        case GamePhase::WAITING: {
            // Start countdown when player is present
            if (ctx.player_id != INVALID_ENTITY && ctx.is_alive(ctx.player_id)) {
                result.set("game_phase", static_cast<int>(GamePhase::COUNTDOWN));
                result.set("round_timer", 0.0f);
                result.log("Game starting...");
            }
            break;
        }
        case GamePhase::COUNTDOWN: {
            int seconds_left = 3 - static_cast<int>(round_timer);
            if (seconds_left != ctx.get<int>("countdown_display", -1)) {
                result.set("countdown_display", seconds_left);
                if (seconds_left > 0) {
                    result.log(std::to_string(seconds_left) + "...");
                    result.play_sound("countdown_beep");
                }
            }
            if (round_timer >= 3.0f) {
                result.set("game_phase", static_cast<int>(GamePhase::PLAYING));
                result.set("round_timer", 0.0f);
                result.log("Round " + std::to_string(current_round) + " - FIGHT!");
                result.play_sound("round_start");
            }
            break;
        }
        case GamePhase::PLAYING: {
            result.set("round_timer", round_timer);

            // Check player death -> round over
            if (!ctx.is_alive(ctx.player_id)) {
                result.set("game_phase", static_cast<int>(GamePhase::ROUND_OVER));
                result.set("round_timer", 0.0f);
                result.log("Round over - Player defeated!");
                result.play_sound("round_lose");
                break;
            }

            // Check all enemies dead -> player wins round
            bool enemies_alive = false;
            auto nearby = ctx.entities_in_radius(ctx.get_position(ctx.player_id), 1000.0f);
            for (auto eid : nearby) {
                if (eid != ctx.player_id && ctx.is_alive(eid)) {
                    enemies_alive = true;
                    break;
                }
            }
            if (!enemies_alive) {
                result.set("game_phase", static_cast<int>(GamePhase::ROUND_OVER));
                result.set("round_timer", 0.0f);
                result.set("player_score", player_score + 100);
                result.log("Round " + std::to_string(current_round) + " complete! +100 points");
                result.play_sound("round_win");
            }

            // Round time limit (120 seconds)
            if (round_timer >= 120.0f) {
                result.set("game_phase", static_cast<int>(GamePhase::ROUND_OVER));
                result.set("round_timer", 0.0f);
                result.log("Time's up!");
            }
            break;
        }
        case GamePhase::ROUND_OVER: {
            // Show scores for 5 seconds, then next round or game over
            if (round_timer >= 5.0f) {
                if (current_round >= max_rounds) {
                    result.set("game_phase", static_cast<int>(GamePhase::GAME_OVER));
                    result.log("Game Over! Final score: " + std::to_string(player_score));
                } else {
                    result.set("current_round", current_round + 1);
                    result.set("game_phase", static_cast<int>(GamePhase::COUNTDOWN));
                    result.set("round_timer", 0.0f);

                    // Respawn player
                    result.heal(ctx.player_id, 100.0f);
                    result.set_transform(ctx.player_id, Transform{.position = vec3{0, 1, 0}});

                    // Respawn enemies
                    result.spawn_entity("enemy_pack_hunter", vec3{-25, 0, 20}, "hunter_respawn");
                    result.spawn_entity("enemy_pack_hunter", vec3{25, 0, 20}, "hunter_respawn");
                    result.spawn_entity("multi_arm_gunner", vec3{30, 1, 30}, "boss_respawn");
                }
            }
            break;
        }
        case GamePhase::GAME_OVER: {
            // Wait for restart input
            if (ctx.is_key_just_pressed("R")) {
                result.set("game_phase", static_cast<int>(GamePhase::WAITING));
                result.set("current_round", 1);
                result.set("player_score", 0);
                result.log("Restarting game...");
            }
            break;
        }
        }

        return result;
    }
};

} // namespace odyssey::demo
