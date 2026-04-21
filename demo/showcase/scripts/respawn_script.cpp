#include "demo/showcase/scripts/respawn_script.h"
#include "scripting/script_registry.h"

#include <string>

namespace odyssey::showcase {

scripting::ScriptResult RespawnScript::tick(const scripting::ScriptContext& ctx) {
    scripting::ScriptResult result;

    const EntityID self = owner_id_;
    if (self == INVALID_ENTITY) return result;

    // Compose the keys once — entity id is part of the namespace so multiple
    // respawnable entities coexist peacefully.
    const std::string id_s     = std::to_string(self);
    const std::string k_point  = "respawn/" + id_s + "/checkpoint";
    const std::string k_health = "respawn/" + id_s + "/spawn_health";
    const std::string k_latch  = "respawn/" + id_s + "/pending";

    const float health    = ctx.get_health(self);
    const bool  pending   = ctx.get<bool>(k_latch, false);

    // Clear the latch once we've been revived (failure path: engine applied
    // the heal and transform last frame).
    if (pending && health > 0.0f) {
        result.set(k_latch, false);
        return result;
    }

    // Only fire while we are dead AND have not yet queued a respawn.
    if (!is_dead(health) || pending) return result;

    // Read the last checkpoint. If nothing has ever touched a checkpoint
    // trigger (i.e. fresh scene load with no pre-populated default) we
    // emit a log and leave the player dead — that is an authoring bug,
    // not a runtime one, and the RespawnScript should be loud about it.
    const vec3 checkpoint = ctx.get<vec3>(k_point, vec3(0.0f));
    const float spawn_hp  = ctx.get<float>(k_health, 100.0f);

    if (!ctx.has_key(k_point)) {
        result.log("RespawnScript: no checkpoint set for entity " + id_s,
                   "warn");
        return result;
    }

    Transform t;
    t.position = checkpoint;
    t.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
    t.scale    = vec3(1.0f);

    result.set_transform(self, t);
    result.heal(self, spawn_hp);
    result.play_sound("respawn", checkpoint, 1.0f);
    result.set(k_latch, true);
    result.log("RespawnScript: respawned entity " + id_s, "info");

    return result;
}

} // namespace odyssey::showcase

REGISTER_SCRIPT(odyssey::showcase::RespawnScript)
