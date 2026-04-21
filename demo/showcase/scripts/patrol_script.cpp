#include "demo/showcase/scripts/patrol_script.h"
#include "scripting/script_registry.h"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace odyssey::showcase {

// ----- pure helpers --------------------------------------------------------

vec3 PatrolScript::nearest_point_on_segment(vec3 p, vec3 a, vec3 b) {
    const vec3 ab = b - a;
    const float denom = glm::dot(ab, ab);
    if (denom <= 1e-8f) return a;
    const float t = glm::clamp(glm::dot(p - a, ab) / denom, 0.0f, 1.0f);
    return a + ab * t;
}

size_t PatrolScript::advance_waypoint(size_t current, size_t count) {
    if (count == 0) return 0;
    return (current + 1u) % count;
}

// ----- tick ----------------------------------------------------------------

scripting::ScriptResult PatrolScript::tick(const scripting::ScriptContext& ctx) {
    scripting::ScriptResult result;

    const EntityID self = owner_id_;
    if (self == INVALID_ENTITY) return result;
    if (!ctx.is_alive(self))    return result;

    // Read waypoints from state_store. Key convention: "patrol/<id>/points".
    // When missing we emit a single log and bail — script survives attach-
    // before-scene-loaded ordering without tripping an error path.
    const std::string key_points = "patrol/" + std::to_string(self) + "/points";
    const std::string key_index  = "patrol/" + std::to_string(self) + "/index";
    const std::string key_speed  = "patrol/" + std::to_string(self) + "/speed";

    const auto waypoints =
        ctx.get<std::vector<vec3>>(key_points, std::vector<vec3>{});
    if (waypoints.size() < 2) {
        // No route assigned yet — idle. This is a valid attach-time state.
        return result;
    }

    const size_t target_idx = std::min(
        static_cast<size_t>(ctx.get<int>(key_index, 0)),
        waypoints.size() - 1);
    const float speed = ctx.get<float>(key_speed, k_default_speed);
    const float dt    = std::max(ctx.delta_time, 0.0f);

    const vec3 pos      = ctx.get_position(self);
    const vec3 target   = waypoints[target_idx];
    const vec3 to_target = target - pos;
    const float dist    = glm::length(to_target);

    // Reached the waypoint → advance and skip motion this frame. Next tick
    // picks up the new segment. Keeps arrival behaviour deterministic.
    if (dist <= k_default_reach_dist) {
        const size_t next_idx = advance_waypoint(target_idx, waypoints.size());
        result.set(key_index, static_cast<int>(next_idx));
        return result;
    }

    // Semi-implicit step: move at most `speed * dt` along the normalised
    // heading. Clamp so we never overshoot the waypoint inside a frame.
    const vec3 dir  = to_target / dist;
    const float step = std::min(speed * dt, dist);
    const vec3 new_pos = pos + dir * step;

    Transform t;
    t.position = new_pos;
    // Face the direction of travel — yaw only (keep upright).
    const float yaw = std::atan2(dir.x, dir.z);
    t.rotation = glm::angleAxis(yaw, vec3(0.f, 1.f, 0.f));
    t.scale    = vec3(1.0f);

    result.set_transform(self, t);
    return result;
}

} // namespace odyssey::showcase

REGISTER_SCRIPT(odyssey::showcase::PatrolScript)
