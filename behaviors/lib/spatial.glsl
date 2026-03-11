// spatial.glsl — Spatial query helpers
// Provides distance queries, direction helpers, line-of-sight checks

#ifndef SPATIAL_GLSL
#define SPATIAL_GLSL

// Distance squared (avoid sqrt for comparisons)
float distance_sq(vec3 a, vec3 b) { vec3 d = b - a; return dot(d, d); }

// Check if point is within radius of center
bool within_radius(vec3 point, vec3 center, float radius) {
    return distance_sq(point, center) <= radius * radius;
}

// Angle between two direction vectors (radians)
float angle_between(vec3 a, vec3 b) {
    float d = dot(normalize(a), normalize(b));
    return acos(clamp(d, -1.0, 1.0));
}

// Check if target is within field of view cone
bool in_field_of_view(vec3 pos, vec3 forward, vec3 target, float fov_radians) {
    vec3 to_target = normalize(target - pos);
    float angle = acos(clamp(dot(forward, to_target), -1.0, 1.0));
    return angle <= fov_radians * 0.5;
}

// Simple line-of-sight check (no occlusion, just distance + angle)
float line_of_sight_score(vec3 pos, vec3 forward, vec3 target, float max_dist, float fov_rad) {
    float dist = length(target - pos);
    if (dist > max_dist) return 0.0;
    vec3 to_target = (target - pos) / max(dist, 0.001);
    float angle = acos(clamp(dot(forward, to_target), -1.0, 1.0));
    if (angle > fov_rad * 0.5) return 0.0;
    float dist_factor = 1.0 - (dist / max_dist);
    float angle_factor = 1.0 - (angle / (fov_rad * 0.5));
    return dist_factor * angle_factor;
}

// Project position onto ground plane (y=0)
vec3 ground_project(vec3 pos) {
    return vec3(pos.x, 0.0, pos.z);
}

// 2D distance (ignoring Y axis)
float distance_2d(vec3 a, vec3 b) {
    vec2 d = a.xz - b.xz;
    return length(d);
}

// Closest point on line segment AB to point P
vec3 closest_point_on_segment(vec3 a, vec3 b, vec3 p) {
    vec3 ab = b - a;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 0.001), 0.0, 1.0);
    return a + t * ab;
}

// Hash function for spatial grid
uint spatial_hash(ivec3 cell, uint grid_size) {
    return uint(cell.x + cell.y * int(grid_size) + cell.z * int(grid_size) * int(grid_size)) % (grid_size * grid_size * grid_size);
}

// World position to grid cell
ivec3 world_to_cell(vec3 pos, float cell_size) {
    return ivec3(floor(pos / cell_size));
}

#endif // SPATIAL_GLSL
