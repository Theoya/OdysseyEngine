#include "fps_humanoid/humanoid_character.h"
#include "animation/skeleton_loader.h"
#include "animation/ik_solver.h"

#include <spdlog/spdlog.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace odyssey {

// ---------------------------------------------------------------------------
// initialize — load skeleton and animation clips from XML assets
// ---------------------------------------------------------------------------
Result<bool> HumanoidCharacter::initialize(const std::filesystem::path& assets_dir) {
    // Load skeleton
    auto skel_result = anim::load_skeleton(assets_dir / "humanoid.skeleton.xml");
    if (skel_result.is_err()) {
        return Result<bool>::err("Failed to load skeleton: " + skel_result.error());
    }
    skeleton_ = std::move(skel_result).value();

    // Load walk cycle
    auto walk_result = anim::load_animation_clip(
        assets_dir / "walk_cycle.anim.xml", skeleton_);
    if (walk_result.is_err()) {
        spdlog::warn("HumanoidCharacter: walk clip not loaded: {}", walk_result.error());
        walk_clip_ = {};
    } else {
        walk_clip_ = std::move(walk_result).value();
    }

    // Load idle
    auto idle_result = anim::load_animation_clip(
        assets_dir / "idle.anim.xml", skeleton_);
    if (idle_result.is_err()) {
        spdlog::warn("HumanoidCharacter: idle clip not loaded: {}", idle_result.error());
        idle_clip_ = {};
    } else {
        idle_clip_ = std::move(idle_result).value();
    }

    // Set up animation player
    anim_player_.set_skeleton(&skeleton_);

    // Start with idle animation
    if (idle_clip_.duration > 0.0f) {
        anim_player_.play(&idle_clip_, 1.0f, true);
    }

    // Default render config: light gray bones, yellow joints
    config_.bone_color = {0.8f, 0.8f, 0.8f, 1.0f};
    config_.joint_color = {1.0f, 0.9f, 0.2f, 1.0f};
    config_.joint_scale = 0.06f;
    config_.render_joints = true;

    // Allocate world transforms
    world_transforms_.resize(skeleton_.bones.size(), mat4(1.0f));

    initialized_ = true;
    spdlog::info("HumanoidCharacter: initialized with {} bones, {} attachments",
                 skeleton_.bones.size(), skeleton_.attachments.size());
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// update — advance animation, compute transforms, build renderables
// ---------------------------------------------------------------------------
void HumanoidCharacter::update(float dt, const vec3& world_pos, const quat& world_rot,
                               float move_speed, float ground_height) {
    if (!initialized_) return;

    position_ = world_pos;
    rotation_ = world_rot;

    // Evaluate animation (speed_factor scales walk animation with movement)
    float speed_factor = std::max(move_speed / 5.0f, 0.5f);
    auto local_transforms = anim_player_.evaluate(dt * speed_factor);

    // Compute world-space transforms from local bone transforms
    world_transforms_ = anim::AnimationPlayer::compute_world_transforms(
        skeleton_, local_transforms);

    // Apply root transform: translate + rotate into world space
    mat4 root_transform = glm::translate(mat4(1.0f), world_pos) *
                          glm::toMat4(world_rot);

    // Simple foot IK adjustment: shift root Y to match ground height
    if (ground_height != 0.0f) {
        root_transform[3][1] += ground_height;
    }

    // Transform all bones from local skeleton space to world space
    for (auto& wt : world_transforms_) {
        wt = root_transform * wt;
    }

    // Convert skeleton + world transforms to renderable entities
    renderables_ = anim::skeleton_to_renderables(skeleton_, world_transforms_, config_);

    // Add gun renderables if visible
    if (gun_visible_) {
        build_gun_renderables();
    }
}

// ---------------------------------------------------------------------------
// get_attachment_transform — compose bone world transform with attachment offset
// ---------------------------------------------------------------------------
mat4 HumanoidCharacter::get_attachment_transform(const std::string& name) const {
    auto it = skeleton_.attachments.find(name);
    if (it == skeleton_.attachments.end()) {
        return mat4(1.0f);
    }

    const auto& attach = it->second;
    if (attach.bone_index >= world_transforms_.size()) {
        return mat4(1.0f);
    }

    // Build local offset matrix from attachment transform
    mat4 local_offset = glm::translate(mat4(1.0f), attach.local_offset.position) *
                        glm::toMat4(attach.local_offset.rotation);

    return world_transforms_[attach.bone_index] * local_offset;
}

// ---------------------------------------------------------------------------
// build_gun_renderables — attach a box (gun) to right_hand_grip
// ---------------------------------------------------------------------------
void HumanoidCharacter::build_gun_renderables() {
    mat4 grip = get_attachment_transform("right_hand_grip");

    // Gun body: elongated dark gray box
    RenderEntity gun{};
    gun.position = vec3(grip[3]); // world position from matrix
    gun.rotation = glm::quat_cast(grip);
    gun.color = {0.25f, 0.25f, 0.28f, 1.0f}; // dark gray
    gun.scale = {0.04f, 0.04f, 0.25f};        // thin and long
    gun.mesh_type = 0; // box

    renderables_.push_back(gun);
}

} // namespace odyssey
