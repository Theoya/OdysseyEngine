// steering.glsl — Steering behaviors for movement
// Include via: #include "steering.glsl"

// Seek: move toward target position
vec3 steer_seek(vec3 current_pos, vec3 target_pos, float max_speed) {
    vec3 desired = target_pos - current_pos;
    float dist = length(desired);
    if (dist < 0.001) return vec3(0.0);
    return (desired / dist) * max_speed;
}

// Flee: move away from threat position
vec3 steer_flee(vec3 current_pos, vec3 threat_pos, float max_speed) {
    vec3 desired = current_pos - threat_pos;
    float dist = length(desired);
    if (dist < 0.001) return vec3(0.0, 0.0, 1.0) * max_speed; // arbitrary escape direction
    return (desired / dist) * max_speed;
}

// Arrive: seek with deceleration near target
vec3 steer_arrive(vec3 current_pos, vec3 target_pos, float max_speed, float slow_radius) {
    vec3 desired = target_pos - current_pos;
    float dist = length(desired);
    if (dist < 0.001) return vec3(0.0);
    float speed = max_speed;
    if (dist < slow_radius) {
        speed = max_speed * (dist / slow_radius);
    }
    return (desired / dist) * speed;
}

// Wander: pseudorandom wandering (uses frame data for variation)
vec3 steer_wander(vec3 forward, float wander_radius, float wander_offset, float jitter, float seed) {
    // Simple hash-based jitter
    float angle = fract(sin(seed * 12.9898 + 78.233) * 43758.5453) * 6.28318;
    vec3 wander_target = forward * wander_offset;
    wander_target.x += cos(angle) * wander_radius * jitter;
    wander_target.z += sin(angle) * wander_radius * jitter;
    float len = length(wander_target);
    if (len < 0.001) return forward;
    return wander_target / len;
}

// Separation: steer away from nearby neighbors
vec3 steer_separate(vec3 current_pos, vec3 neighbor_center, float count, float max_speed) {
    if (count < 0.5) return vec3(0.0); // no neighbors
    vec3 away = current_pos - neighbor_center;
    float dist = length(away);
    if (dist < 0.001) return vec3(0.0, 1.0, 0.0) * max_speed;
    return (away / dist) * max_speed;
}

// Cohesion: steer toward center of nearby neighbors
vec3 steer_cohesion(vec3 current_pos, vec3 neighbor_center, float count, float max_speed) {
    if (count < 0.5) return vec3(0.0);
    return steer_seek(current_pos, neighbor_center, max_speed);
}

// Alignment: match heading with nearby neighbors
vec3 steer_alignment(vec3 current_velocity, vec3 neighbor_avg_velocity, float count, float max_speed) {
    if (count < 0.5) return vec3(0.0);
    vec3 desired = neighbor_avg_velocity;
    float len = length(desired);
    if (len < 0.001) return vec3(0.0);
    return (desired / len) * max_speed;
}

// Classic flocking: separation + cohesion + alignment
vec3 steer_flock(
    vec3 pos, vec3 vel,
    vec3 neighbor_center, vec3 neighbor_avg_vel, float neighbor_count,
    float max_speed,
    float sep_weight, float coh_weight, float ali_weight
) {
    vec3 sep = steer_separate(pos, neighbor_center, neighbor_count, max_speed) * sep_weight;
    vec3 coh = steer_cohesion(pos, neighbor_center, neighbor_count, max_speed) * coh_weight;
    vec3 ali = steer_alignment(vel, neighbor_avg_vel, neighbor_count, max_speed) * ali_weight;

    vec3 combined = sep + coh + ali;
    float len = length(combined);
    if (len > max_speed) combined = (combined / len) * max_speed;
    return combined;
}

// Direction toward a target, normalized
vec3 direction_to(vec3 from, vec3 to) {
    vec3 d = to - from;
    float len = length(d);
    if (len < 0.001) return vec3(0.0);
    return d / len;
}

// Distance between two points (convenience)
float distance_to(vec3 a, vec3 b) {
    return length(b - a);
}
