#pragma once

#include "scripting/script.h"
#include "core/types.h"
#include <vector>

namespace odyssey::showcase {

// PatrolScript
// -----------
// Drives an entity along a closed polyline of waypoints at a constant linear
// speed. Demonstrates the runtime attach story: an agent or the Editor's
// Script Console can call ScriptRunner::attach_script("PatrolScript", eid)
// on any entity, and the next tick this script starts steering it.
//
// Pure-by-contract: all motion is expressed as a SetTransformMutation in the
// returned ScriptResult. Waypoint progression is derived from elapsed time
// (ctx.total_time) plus the entity's current position — we never store
// wall-clock state on the script instance itself, so restarts and replays
// are deterministic.
//
// Waypoints are loaded from a key in ctx state_store under the conventional
// prefix "patrol/<entity_id>/points" as a std::vector<vec3>. The showcase
// scene XML populates these at load time; the Script Console can rewrite
// them live via ScriptResult::set().
class PatrolScript : public scripting::Script {
public:
    std::string name() const override { return "PatrolScript"; }

    scripting::ScriptResult tick(const scripting::ScriptContext& ctx) override;

    // Exposed for /physics-step and unit tests — pure helpers.
    static vec3 nearest_point_on_segment(vec3 p, vec3 a, vec3 b);
    static size_t advance_waypoint(size_t current, size_t count);

private:
    // Tunables read from the state_store each tick (hot-editable from the
    // Script Console without re-attaching). Defaults are chosen for the
    // showcase scout archetype.
    static constexpr float k_default_speed      = 3.0f;  // m/s
    static constexpr float k_default_reach_dist = 0.35f; // m
};

} // namespace odyssey::showcase
