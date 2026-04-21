#pragma once

#include "scripting/script.h"
#include "core/types.h"

namespace odyssey::showcase {

// RespawnScript
// -------------
// Respawns the player at the last checkpoint the moment health hits zero.
//
// State contract (all stored in ScriptContext::state_store):
//   "respawn/<id>/checkpoint"       vec3   — last touched checkpoint position
//   "respawn/<id>/spawn_health"     float  — HP to restore on respawn
//   "respawn/<id>/pending"          bool   — internal latch: awaiting respawn
//
// Pure: we never snapshot wall-clock state on the script instance. A reset
// is a single SetTransform + Heal pair emitted in the frame we observe
// health <= 0, flipped behind a "pending" latch so we only fire once per
// death. The latch clears the next tick when health is restored, so the
// script self-disarms without an explicit sentinel from the engine.
class RespawnScript : public scripting::Script {
public:
    std::string name() const override { return "RespawnScript"; }

    scripting::ScriptResult tick(const scripting::ScriptContext& ctx) override;

    // Pure helper — exposed for unit test.
    static bool is_dead(float health) { return health <= 0.0f; }
};

} // namespace odyssey::showcase
