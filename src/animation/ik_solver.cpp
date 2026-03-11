#include "animation/ik_solver.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace odyssey::anim {

TwoBoneIKResult solve_two_bone_ik(
    vec3 upper_pos, vec3 mid_pos, vec3 end_pos,
    vec3 target, vec3 pole_vector,
    float upper_length, float lower_length) {

    TwoBoneIKResult result{};
    result.upper_local_rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
    result.lower_local_rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
    result.reached = false;

    float max_reach = upper_length + lower_length;
    float min_reach = std::fabs(upper_length - lower_length);

    vec3 to_target = target - upper_pos;
    float target_dist = glm::length(to_target);

    // Avoid degenerate cases
    if (target_dist < 1e-6f) return result;

    // Clamp distance to reachable range
    bool clamped = false;
    if (target_dist >= max_reach - 1e-6f) {
        target_dist = max_reach - 1e-6f;
        clamped = true;
    } else if (target_dist <= min_reach + 1e-6f) {
        target_dist = min_reach + 1e-6f;
        clamped = true;
    }

    result.reached = !clamped;

    vec3 target_dir = glm::normalize(to_target);

    // Reconstruct target position if clamped
    vec3 effective_target = upper_pos + target_dir * target_dist;

    // --- Law of cosines to find angle at mid joint ---
    // Triangle: upper_length, lower_length, target_dist
    // Angle at upper joint (alpha): cos(alpha) = (a^2 + c^2 - b^2) / (2ac)
    //   where a = upper_length, b = lower_length, c = target_dist
    float a = upper_length;
    float b = lower_length;
    float c = target_dist;

    float cos_upper = (a * a + c * c - b * b) / (2.0f * a * c);
    cos_upper = std::clamp(cos_upper, -1.0f, 1.0f);
    float angle_upper = std::acos(cos_upper);

    // Angle at mid joint (beta): cos(beta) = (a^2 + b^2 - c^2) / (2ab)
    float cos_mid = (a * a + b * b - c * c) / (2.0f * a * b);
    cos_mid = std::clamp(cos_mid, -1.0f, 1.0f);
    float angle_mid = std::acos(cos_mid);

    // --- Determine bend plane using pole vector ---
    // The initial bone direction
    vec3 initial_upper_dir = glm::normalize(mid_pos - upper_pos);
    vec3 initial_chain_dir = glm::normalize(end_pos - upper_pos);

    // Rotation to align chain direction to target direction
    quat aim_rotation;
    float dot_val = glm::dot(initial_chain_dir, target_dir);
    if (dot_val > 0.9999f) {
        aim_rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
    } else if (dot_val < -0.9999f) {
        // 180 degree rotation - find a perpendicular axis
        vec3 perp = glm::cross(initial_chain_dir, vec3(1.0f, 0.0f, 0.0f));
        if (glm::length(perp) < 1e-6f) {
            perp = glm::cross(initial_chain_dir, vec3(0.0f, 1.0f, 0.0f));
        }
        perp = glm::normalize(perp);
        aim_rotation = glm::angleAxis(glm::pi<float>(), perp);
    } else {
        vec3 axis = glm::normalize(glm::cross(initial_chain_dir, target_dir));
        float angle = std::acos(std::clamp(dot_val, -1.0f, 1.0f));
        aim_rotation = glm::angleAxis(angle, axis);
    }

    // Determine the bend plane from pole vector
    // Project pole vector onto the plane perpendicular to target_dir
    vec3 pole_dir = pole_vector - upper_pos;
    vec3 pole_on_plane = pole_dir - target_dir * glm::dot(pole_dir, target_dir);
    float pole_len = glm::length(pole_on_plane);

    // Compute the rotated upper bone direction before bend-plane correction
    vec3 rotated_upper_dir = aim_rotation * initial_upper_dir;

    // Project rotated_upper_dir onto plane perpendicular to target_dir
    vec3 upper_on_plane = rotated_upper_dir - target_dir * glm::dot(rotated_upper_dir, target_dir);
    float upper_plane_len = glm::length(upper_on_plane);

    // Twist rotation to align the bend plane with the pole vector
    quat twist_rotation(1.0f, 0.0f, 0.0f, 0.0f);
    if (pole_len > 1e-6f && upper_plane_len > 1e-6f) {
        vec3 pole_norm = glm::normalize(pole_on_plane);
        vec3 upper_norm = glm::normalize(upper_on_plane);
        float twist_dot = std::clamp(glm::dot(upper_norm, pole_norm), -1.0f, 1.0f);
        float twist_angle = std::acos(twist_dot);
        vec3 twist_cross = glm::cross(upper_norm, pole_norm);
        if (glm::dot(twist_cross, target_dir) < 0.0f) {
            twist_angle = -twist_angle;
        }
        twist_rotation = glm::angleAxis(twist_angle, target_dir);
    }

    // The upper bone rotates by: aim + twist + upper_angle offset
    // Upper bone needs to point angle_upper away from the target direction in the bend plane
    vec3 bend_axis = (pole_len > 1e-6f)
        ? glm::normalize(glm::cross(target_dir, glm::normalize(pole_on_plane)))
        : vec3(0.0f, 0.0f, 1.0f);

    quat upper_bend = glm::angleAxis(-angle_upper, bend_axis);
    quat upper_world = twist_rotation * aim_rotation * upper_bend;

    // Compute what the initial upper rotation was
    // The upper bone local rotation is relative to rest pose
    result.upper_local_rotation = upper_world;

    // The lower bone bends by pi - angle_mid relative to straight
    float lower_bend_angle = glm::pi<float>() - angle_mid;
    result.lower_local_rotation = glm::angleAxis(lower_bend_angle, bend_axis);

    return result;
}

} // namespace odyssey::anim
