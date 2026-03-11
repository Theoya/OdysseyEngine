#include "animation/skeleton_renderer.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace odyssey::anim {

std::vector<RenderEntity> skeleton_to_renderables(
    const Skeleton& skel, const std::vector<mat4>& world_transforms,
    const SkeletonRenderConfig& config) {

    std::vector<RenderEntity> entities;

    if (world_transforms.size() < skel.bones.size()) return entities;

    // Reserve space: up to one cylinder per bone + one sphere per bone
    entities.reserve(skel.bones.size() * 2);

    for (size_t i = 0; i < skel.bones.size(); ++i) {
        const auto& bone = skel.bones[i];

        // Draw cylinder for bones that have a parent
        if (bone.parent_index >= 0) {
            size_t parent_idx = static_cast<size_t>(bone.parent_index);

            // Extract world positions from transform matrices (column 3)
            vec3 parent_pos = vec3(world_transforms[parent_idx][3]);
            vec3 bone_pos = vec3(world_transforms[i][3]);

            vec3 diff = bone_pos - parent_pos;
            float length = glm::length(diff);

            if (length > 1e-6f) {
                // Midpoint for cylinder placement
                vec3 midpoint = (parent_pos + bone_pos) * 0.5f;

                // Compute rotation: align Y-axis to bone direction
                vec3 direction = glm::normalize(diff);
                vec3 up(0.0f, 1.0f, 0.0f);

                quat rotation;
                float dot_val = glm::dot(up, direction);
                if (dot_val > 0.9999f) {
                    rotation = quat(1.0f, 0.0f, 0.0f, 0.0f); // identity
                } else if (dot_val < -0.9999f) {
                    // 180 degree rotation around Z
                    rotation = glm::angleAxis(glm::pi<float>(), vec3(0.0f, 0.0f, 1.0f));
                } else {
                    vec3 axis = glm::normalize(glm::cross(up, direction));
                    float angle = std::acos(std::clamp(dot_val, -1.0f, 1.0f));
                    rotation = glm::angleAxis(angle, axis);
                }

                RenderEntity cylinder{};
                cylinder.position = midpoint;
                cylinder.rotation = rotation;
                cylinder.scale = vec3(bone.render_radius * 2.0f, length, bone.render_radius * 2.0f);
                cylinder.mesh_type = 3; // CYLINDER
                cylinder.color = config.bone_color;
                entities.push_back(cylinder);
            }
        }

        // Draw sphere at each joint
        if (config.render_joints) {
            vec3 joint_pos = vec3(world_transforms[i][3]);

            RenderEntity sphere{};
            sphere.position = joint_pos;
            sphere.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
            sphere.scale = vec3(config.joint_scale);
            sphere.mesh_type = 1; // SPHERE
            sphere.color = config.joint_color;
            entities.push_back(sphere);
        }
    }

    return entities;
}

std::vector<RenderEntity> attachment_to_renderables(
    const Skeleton& skel, const std::vector<mat4>& world_transforms,
    const std::string& attachment_name,
    const std::vector<RenderEntity>& object_template) {

    std::vector<RenderEntity> entities;

    auto it = skel.attachments.find(attachment_name);
    if (it == skel.attachments.end()) return entities;

    const auto& attachment = it->second;
    if (attachment.bone_index >= world_transforms.size()) return entities;

    // Get bone world transform
    mat4 bone_world = world_transforms[attachment.bone_index];

    // Build attachment local offset matrix
    mat4 offset_mat = glm::translate(mat4(1.0f), attachment.local_offset.position);
    offset_mat *= glm::mat4_cast(attachment.local_offset.rotation);
    offset_mat = glm::scale(offset_mat, attachment.local_offset.scale);

    // Combined attachment world transform
    mat4 attach_world = bone_world * offset_mat;

    // Extract attachment world position and rotation
    vec3 attach_pos = vec3(attach_world[3]);
    // Extract rotation from the upper-left 3x3 (assuming uniform scale on attachment offset)
    mat4 rotation_mat = attach_world;
    // Remove translation
    rotation_mat[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    // Remove scale from columns
    vec3 col0 = vec3(rotation_mat[0]);
    vec3 col1 = vec3(rotation_mat[1]);
    vec3 col2 = vec3(rotation_mat[2]);
    float scale_x = glm::length(col0);
    float scale_y = glm::length(col1);
    float scale_z = glm::length(col2);
    if (scale_x > 1e-6f) rotation_mat[0] = vec4(col0 / scale_x, 0.0f);
    if (scale_y > 1e-6f) rotation_mat[1] = vec4(col1 / scale_y, 0.0f);
    if (scale_z > 1e-6f) rotation_mat[2] = vec4(col2 / scale_z, 0.0f);
    quat attach_rot = glm::quat_cast(rotation_mat);

    entities.reserve(object_template.size());
    for (const auto& tmpl : object_template) {
        RenderEntity entity = tmpl;

        // Transform template entity by attachment world transform
        // Rotate and translate the template position
        vec3 rotated_pos = attach_rot * tmpl.position;
        entity.position = attach_pos + rotated_pos;
        entity.rotation = attach_rot * tmpl.rotation;
        // Scale is kept from template (object scale is independent of attachment)

        entities.push_back(entity);
    }

    return entities;
}

} // namespace odyssey::anim
