#pragma once
#include "core/types.h"

namespace odyssey::anim {

struct TwoBoneIKResult {
    quat upper_local_rotation;
    quat lower_local_rotation;
    bool reached;
};

// Solve two-bone IK (e.g., hip->knee->foot) using law of cosines.
// upper_pos, mid_pos, end_pos: current world positions of the 3 joints
// target: desired end effector position
// pole_vector: hint for bend plane direction
TwoBoneIKResult solve_two_bone_ik(
    vec3 upper_pos, vec3 mid_pos, vec3 end_pos,
    vec3 target, vec3 pole_vector,
    float upper_length, float lower_length);

} // namespace odyssey::anim
