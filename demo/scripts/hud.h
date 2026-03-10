#pragma once

#include "scripting/script.h"
#include <string>
#include <sstream>
#include <iomanip>

namespace odyssey::demo {

// HUD script — reads game state, outputs display mutations.
// Pure: ScriptContext in -> ScriptResult out.
class HUD : public scripting::Script {
public:
    std::string name() const override { return "HUD"; }

    scripting::ScriptResult tick(const scripting::ScriptContext& ctx) override {
        scripting::ScriptResult result;

        // Read player stats
        float health = 0.0f;
        float max_health = 100.0f;
        float ammo = 0.0f;
        if (ctx.player_id != INVALID_ENTITY) {
            health = ctx.get_health(ctx.player_id);
            max_health = ctx.get<float>("player_max_health", 100.0f);
            ammo = ctx.get<float>("player_ammo", 0.0f);
        }

        // Read game state
        int phase = ctx.get<int>("game_phase", 0);
        int round = ctx.get<int>("current_round", 1);
        int max_rounds = ctx.get<int>("max_rounds", 3);
        int score = ctx.get<int>("player_score", 0);
        float round_timer = ctx.get<float>("round_timer", 0.0f);
        int countdown = ctx.get<int>("countdown_display", 0);

        // Build HUD text as state updates (consumed by rendering system)
        std::ostringstream hud;

        // Health bar
        float health_pct = (max_health > 0) ? (health / max_health) * 100.0f : 0.0f;
        hud << "HP: " << static_cast<int>(health) << "/" << static_cast<int>(max_health)
            << " [" << std::string(static_cast<size_t>(health_pct / 5), '|')
            << std::string(20 - static_cast<size_t>(health_pct / 5), ' ') << "]";
        result.set("hud_health", hud.str());
        hud.str("");

        // Ammo
        result.set("hud_ammo", "AMMO: " + std::to_string(static_cast<int>(ammo)));

        // Round info
        result.set("hud_round", "Round " + std::to_string(round) + "/" + std::to_string(max_rounds));

        // Score
        result.set("hud_score", "Score: " + std::to_string(score));

        // Timer
        int timer_seconds = static_cast<int>(round_timer);
        int mins = timer_seconds / 60;
        int secs = timer_seconds % 60;
        hud << std::setfill('0') << std::setw(2) << mins << ":"
            << std::setfill('0') << std::setw(2) << secs;
        result.set("hud_timer", hud.str());

        // Phase-specific messages
        switch (static_cast<GamePhase>(phase)) {
        case GamePhase::WAITING:
            result.set("hud_message", "Waiting for players...");
            break;
        case GamePhase::COUNTDOWN:
            result.set("hud_message", std::to_string(countdown));
            break;
        case GamePhase::PLAYING:
            result.set("hud_message", "");
            break;
        case GamePhase::ROUND_OVER:
            result.set("hud_message", "Round Complete!");
            break;
        case GamePhase::GAME_OVER:
            result.set("hud_message", "GAME OVER - Press R to restart");
            break;
        }

        // Crosshair state (changes color based on target)
        result.set("hud_crosshair_active", ctx.get<bool>("has_target", false));

        return result;
    }

private:
    // GamePhase enum duplicated here since HUD needs it
    // In production, this would be in a shared header
    enum class GamePhase { WAITING, COUNTDOWN, PLAYING, ROUND_OVER, GAME_OVER };
};

} // namespace odyssey::demo
