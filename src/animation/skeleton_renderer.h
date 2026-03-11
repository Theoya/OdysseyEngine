#pragma once
#include "animation/skeleton.h"
#include "app/game.h"  // for RenderEntity
#include <vector>

namespace odyssey::anim {

struct SkeletonRenderConfig {
    vec4 bone_color{0.8f, 0.8f, 0.8f, 1.0f};
    vec4 joint_color{1.0f, 0.9f, 0.2f, 1.0f};
    float joint_scale = 0.06f;
    bool render_joints = true;
};

// Convert skeleton + world transforms to RenderEntities (cylinders for bones, spheres for joints)
std::vector<RenderEntity> skeleton_to_renderables(
    const Skeleton& skel, const std::vector<mat4>& world_transforms,
    const SkeletonRenderConfig& config = {});

// Render objects attached to a bone (e.g., gun at right_hand_grip)
std::vector<RenderEntity> attachment_to_renderables(
    const Skeleton& skel, const std::vector<mat4>& world_transforms,
    const std::string& attachment_name,
    const std::vector<RenderEntity>& object_template);

} // namespace odyssey::anim
