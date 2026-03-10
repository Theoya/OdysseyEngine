#pragma once

#include "scripting/script.h"
#include <cmath>

namespace odyssey::demo {

// Player controller — translates keyboard/mouse input into the persist buffer
// format that player_input.nadir reads on the GPU.
// Pure: ScriptContext in -> ScriptResult out.
class PlayerController : public scripting::Script {
public:
    std::string name() const override { return "PlayerController"; }

    scripting::ScriptResult tick(const scripting::ScriptContext& ctx) override {
        scripting::ScriptResult result;

        if (ctx.player_id == INVALID_ENTITY) return result;
        if (!ctx.is_alive(ctx.player_id)) return result;

        // Read input
        float forward = 0.0f;
        float strafe = 0.0f;
        if (ctx.is_key_pressed("W")) forward += 1.0f;
        if (ctx.is_key_pressed("S")) forward -= 1.0f;
        if (ctx.is_key_pressed("A")) strafe -= 1.0f;
        if (ctx.is_key_pressed("D")) strafe += 1.0f;

        // Normalize diagonal movement
        float input_len = std::sqrt(forward * forward + strafe * strafe);
        if (input_len > 1.0f) {
            forward /= input_len;
            strafe /= input_len;
        }

        // Mouse look (yaw stored as cumulative, managed by engine input system)
        float look_yaw = ctx.get<float>("player_look_yaw", 0.0f);
        float shoot = ctx.is_key_pressed("MOUSE1") ? 1.0f : 0.0f;

        // Write input state to persist buffer slots (read by player_input.nadir)
        // memory_0.x = forward, memory_0.y = strafe, memory_0.z = yaw, memory_0.w = shoot
        result.set("player_input_forward", forward);
        result.set("player_input_strafe", strafe);
        result.set("player_input_yaw", look_yaw);
        result.set("player_input_shoot", shoot);

        // Sprint
        if (ctx.is_key_pressed("SHIFT")) {
            result.set("player_sprint", true);
        }

        // Reload
        if (ctx.is_key_just_pressed("R")) {
            float ammo = ctx.get<float>("player_ammo", 0.0f);
            if (ammo < 120.0f) {
                result.set("player_ammo", 120.0f);
                result.play_sound("reload");
                result.log("Reloading...");
            }
        }

        // Interact
        if (ctx.is_key_just_pressed("E")) {
            vec3 player_pos = ctx.get_position(ctx.player_id);
            auto nearby = ctx.entities_in_radius(player_pos, 3.0f);
            for (auto eid : nearby) {
                if (eid != ctx.player_id) {
                    result.set("interact_target", static_cast<int>(eid));
                    break;
                }
            }
        }

        return result;
    }
};

} // namespace odyssey::demo
