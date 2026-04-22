#pragma once
#include "core/types.h"
#include "scripting/script.h"
#include "scripting/script_context.h"
#include <string>

namespace odyssey::scripting {

// Phase 9: FirstPersonController — enables WASD + mouse for walking + jumping
// Implements the "weighted and deliberate" movement pillar from Fantasy-Etherealism.
class FirstPersonController : public Script {
public:
    // Movement tuned for "Fantasy-Etherealism weighted and deliberate" vibe
    static constexpr float kWalkSpeed = 4.0f;        // m/s — brisk but deliberate
    static constexpr float kJumpImpulse = 5.5f;       // m/s upward (~1.5m height)
    static constexpr float kGravity = 9.81f;          // m/s² — Earth standard
    static constexpr float kEyeHeight = 1.7f;         // m — head position relative to feet
    static constexpr float kMouseSensitivity = 0.002f; // rad/px
    static constexpr float kHeadBobAmplitude = 0.02f;  // m — subtle motion
    static constexpr float kHeadBobFrequency = 2.0f;   // Hz when walking

    // Script interface (tick is not used; pre/post physics called by game)
    ScriptResult tick(const ScriptContext& ctx) override { return ScriptResult(); }
    std::string name() const override { return "FirstPersonController"; }

    // Phase 9: Script lifecycle methods called by game's tick loop
    // pre_physics: reads input, writes velocity/yaw to rigidbody
    ScriptResult pre_physics(ScriptContext& ctx);

    // post_physics: reads body position, writes camera pitch + head-bob offset
    ScriptResult post_physics(ScriptContext& ctx);

    // State tracked across frames
    float yaw = 0.0f;      // horizontal rotation (radians)
    float pitch = 0.0f;    // vertical rotation (radians)
    float head_bob_offset = 0.0f;  // vertical head-bob displacement
    bool was_grounded_last_frame = false;

    // Phase 10+ AUDIO-HOOK (deferred): ground-contact transitions should fire
    // footstep_event payload: { surface_type: enum, velocity: float, entity_id: EntityID }
    // Will be consumed by a future SfxDirector subsystem (new council vote).
};

} // namespace odyssey::scripting
