#include "fps_humanoid/berserk_halo_character.h"
#include "animation/skeleton_loader.h"

#include <spdlog/spdlog.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <cmath>

namespace odyssey {

// Derived bone REST-pose world positions from humanoid.skeleton.xml, used to
// convert the Blender world-space primitive positions into bone-local offsets.
// (All offsets below are pre-computed rather than recomputed at runtime — the
// skeleton is static data.)
namespace {
constexpr float kRootY  = 0.95f;
constexpr float kSpineY = 1.15f;
constexpr float kChestY = 1.35f;
constexpr float kHeadY  = 1.65f;   // neck(+0.1) then head's bone root

// Helper: build a piece at world-space authoring position by subtracting the
// named bone's rest world position to get the bone-local offset.
// When the bone is later animated, the bone's world_transform carries the
// rotation/translation so the piece follows.
Transform piece_local(const vec3& world_pos_authored, const vec3& bone_world_rest,
                      const quat& rot = quat(1, 0, 0, 0)) {
    Transform t;
    t.position = world_pos_authored - bone_world_rest;
    t.rotation = rot;
    return t;
}

vec3 shade(const vec4& base, float darken) {
    return vec3(base.r * (1.0f - darken), base.g * (1.0f - darken), base.b * (1.0f - darken));
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// initialize — load skeleton + idle (from fps_humanoid) + heavy walk (from
// berserk_halo_mk3 assets dir).
// ---------------------------------------------------------------------------
Result<bool> BerserkHaloCharacter::initialize(
    const std::filesystem::path& humanoid_assets_dir,
    const std::filesystem::path& berserk_assets_dir) {

    auto skel_result = anim::load_skeleton(humanoid_assets_dir / "humanoid.skeleton.xml");
    if (skel_result.is_err()) {
        return Result<bool>::err("BerserkHalo: failed to load skeleton: " + skel_result.error());
    }
    skeleton_ = std::move(skel_result).value();

    auto walk_result = anim::load_animation_clip(
        berserk_assets_dir / "berserk_halo_walk.anim.xml", skeleton_);
    if (walk_result.is_err()) {
        spdlog::warn("BerserkHalo: heavy walk not loaded: {}", walk_result.error());
        walk_clip_ = {};
    } else {
        walk_clip_ = std::move(walk_result).value();
    }

    auto idle_result = anim::load_animation_clip(
        humanoid_assets_dir / "idle.anim.xml", skeleton_);
    if (idle_result.is_err()) {
        spdlog::warn("BerserkHalo: idle not loaded: {}", idle_result.error());
        idle_clip_ = {};
    } else {
        idle_clip_ = std::move(idle_result).value();
    }

    anim_player_.set_skeleton(&skeleton_);
    if (idle_clip_.duration > 0.0f) {
        anim_player_.play(&idle_clip_, 1.0f, true);
    }

    world_transforms_.resize(skeleton_.bones.size(), mat4(1.0f));
    build_piece_manifest();

    initialized_ = true;
    spdlog::info("BerserkHaloCharacter: initialized with {} bones, {} armor pieces",
                 skeleton_.bones.size(), pieces_.size());
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// build_piece_manifest — the armor primitive layout.
//
// Each ArmorPiece.local_offset is authored in Blender world-space above then
// converted to bone-local by subtracting the bone's rest world position.
// Local_rot is the primitive's orientation in bone-local space (identity except
// for horns / cheek plates / pauldron lean).
//
// Mesh types: 0=box, 1=sphere, 3=cylinder. Scales are HALF-extents for boxes
// (mesh_type=0 is a 1x1x1 primitive), RADIUS+HEIGHT for cylinder (x=r, y=h, z=r).
// The renderer scales the primitive uniformly via RenderEntity::scale.
// ---------------------------------------------------------------------------
void BerserkHaloCharacter::build_piece_manifest() {
    pieces_.clear();

    // ========== HELMET GROUP (parented to 'head') ==========
    const vec3 head_rest(0, kHeadY, 0);
    // 1. Cranium shell — predatory skull box
    pieces_.push_back({"head",
        vec3(0, 1.60f, 0.02f) - head_rest,
        quat(1, 0, 0, 0),
        vec3(0.26f, 0.30f, 0.30f), 0, 0.0f});

    // 2. Horn L (forward-swept cone) — approximate as a cylinder rotated
    //    around the X axis -55deg, with Z tilt ±12deg.
    {
        quat r = glm::angleAxis(glm::radians(-55.0f), vec3(1, 0, 0))
               * glm::angleAxis(glm::radians(12.0f), vec3(0, 0, 1));
        pieces_.push_back({"head",
            vec3(-0.11f, 1.78f, 0.08f) - head_rest, r,
            vec3(0.03f, 0.30f, 0.03f), 3, -0.2f});  // slightly brighter than base
    }
    // 3. Horn R
    {
        quat r = glm::angleAxis(glm::radians(-55.0f), vec3(1, 0, 0))
               * glm::angleAxis(glm::radians(-12.0f), vec3(0, 0, 1));
        pieces_.push_back({"head",
            vec3(0.11f, 1.78f, 0.08f) - head_rest, r,
            vec3(0.03f, 0.30f, 0.03f), 3, -0.2f});
    }
    // 4. Visor slit — thin bright strip (own color in compose_piece).
    pieces_.push_back({"head",
        vec3(0, 1.60f, 0.18f) - head_rest,
        quat(1, 0, 0, 0),
        vec3(0.22f, 0.020f, 0.01f), 0, 0.0f});  // [tagged VISOR below via index]
    // 5. Cheek L — slight Y-yaw inward.
    {
        quat r = glm::angleAxis(glm::radians(-15.0f), vec3(0, 1, 0));
        pieces_.push_back({"head",
            vec3(-0.11f, 1.52f, 0.11f) - head_rest, r,
            vec3(0.09f, 0.10f, 0.14f), 0, 0.05f});
    }
    // 6. Cheek R
    {
        quat r = glm::angleAxis(glm::radians(15.0f), vec3(0, 1, 0));
        pieces_.push_back({"head",
            vec3(0.11f, 1.52f, 0.11f) - head_rest, r,
            vec3(0.09f, 0.10f, 0.14f), 0, 0.05f});
    }

    // ========== TORSO GROUP (parented to 'chest' = 1.35 world Y) ==========
    const vec3 chest_rest(0, kChestY, 0);
    const quat hunch = glm::angleAxis(glm::radians(3.0f), vec3(1, 0, 0));

    // 7. Sternum ridge
    pieces_.push_back({"chest",
        vec3(0, 1.28f, 0.14f) - chest_rest, hunch,
        vec3(0.04f, 0.32f, 0.06f), 0, -0.05f});

    // 8 & 9. Chest upper L/R — chevron outward
    for (int i = 0; i < 2; ++i) {
        int xs = (i == 0) ? -1 : 1;
        quat r = hunch * glm::angleAxis(glm::radians(static_cast<float>(xs) * -18.0f), vec3(0, 0, 1));
        pieces_.push_back({"chest",
            vec3(xs * 0.14f, 1.38f, 0.08f) - chest_rest, r,
            vec3(0.28f, 0.18f, 0.18f), 0, 0.0f});
    }

    // 10 & 11. Chest lower L/R — second chevron
    for (int i = 0; i < 2; ++i) {
        int xs = (i == 0) ? -1 : 1;
        quat r = hunch * glm::angleAxis(glm::radians(static_cast<float>(xs) * -12.0f), vec3(0, 0, 1));
        pieces_.push_back({"chest",
            vec3(xs * 0.13f, 1.22f, 0.07f) - chest_rest, r,
            vec3(0.26f, 0.14f, 0.18f), 0, 0.05f});
    }

    // 12-14. Rib segments (tapering slabs) — attach to chest (pivot fine).
    const float rib_ys[3] = {1.08f, 1.00f, 0.93f};
    const float rib_ws[3] = {0.44f, 0.41f, 0.38f};
    for (int i = 0; i < 3; ++i) {
        pieces_.push_back({"chest",
            vec3(0, rib_ys[i], 0.04f) - chest_rest, hunch,
            vec3(rib_ws[i], 0.055f, 0.22f), 0, 0.10f + 0.03f * i});
    }

    // ========== BACK GROUP (to 'chest' for hump/scapula; to 'spine' for vertebra) ==========
    const vec3 spine_rest(0, kSpineY, 0);

    // 23. Spine vertebrae — 7 small boxes down the back (chain relative to spine).
    for (int i = 0; i < 7; ++i) {
        float y = 1.50f - i * 0.09f;
        pieces_.push_back({"spine",
            vec3(0, y, -0.14f) - spine_rest, quat(1, 0, 0, 0),
            vec3(0.08f, 0.06f, 0.08f), 0, -0.05f});
    }
    // 24. Scapula L/R — trapezoid plates.
    for (int i = 0; i < 2; ++i) {
        int xs = (i == 0) ? -1 : 1;
        quat r = glm::angleAxis(glm::radians(-8.0f), vec3(1, 0, 0))
               * glm::angleAxis(glm::radians(static_cast<float>(xs) * -6.0f), vec3(0, 0, 1));
        pieces_.push_back({"chest",
            vec3(xs * 0.18f, 1.42f, -0.12f) - chest_rest, r,
            vec3(0.22f, 0.24f, 0.06f), 0, 0.05f});
    }
    // 25. Back hump — dome.
    pieces_.push_back({"chest",
        vec3(0, 1.48f, -0.20f) - chest_rest, quat(1, 0, 0, 0),
        vec3(0.36f, 0.30f, 0.25f), 1, -0.05f});

    // ========== SHOULDER + ARM GROUPS ==========
    for (int side_idx = 0; side_idx < 2; ++side_idx) {
        int xs = (side_idx == 0) ? -1 : 1;
        const std::string side_suffix = (xs < 0) ? "_l" : "_r";

        // Pauldron stack — parented to 'chest' (not shoulder, because shoulder bone
        // is tiny and pauldrons cover it anyway).
        for (int i = 0; i < 3; ++i) {
            float w = 0.22f - i * 0.03f;
            float h = 0.10f - i * 0.015f;
            float y = 1.45f - i * 0.07f;
            quat r = hunch * glm::angleAxis(glm::radians(static_cast<float>(xs) * -20.0f), vec3(0, 0, 1));
            pieces_.push_back({"chest",
                vec3(xs * 0.26f, y, 0.02f) - chest_rest, r,
                vec3(w * 1.28f, h, 0.22f * 1.28f), 0, 0.02f * i});
        }

        // Upper arm cylinder — parented to 'upper_arm_l/r' bone.
        const std::string ua = "upper_arm" + side_suffix;
        // upper_arm rest world pos = shoulder + (0, -0.28, 0) = (±0.15, 1.07, 0)
        // upper_arm bone LENGTH is 0.28 and POINTS DOWN the -Y axis in bone-local space.
        // The bone's world_transform's origin is at the TOP of the upper arm (the joint
        // above the elbow), so a piece at local (0, -0.14, 0) sits mid-upper-arm.
        pieces_.push_back({ua,
            vec3(0, -0.14f, 0), quat(1, 0, 0, 0),
            vec3(0.15f, 0.26f, 0.15f), 3, 0.10f});

        // Elbow lobster — 3 disks, parented to lower_arm's TOP (local origin).
        const std::string la = "lower_arm" + side_suffix;
        for (int i = 0; i < 3; ++i) {
            pieces_.push_back({la,
                vec3(0, 0.0f - i * 0.025f, 0), quat(1, 0, 0, 0),
                vec3(0.17f - i * 0.01f, 0.035f, 0.17f - i * 0.01f), 3, -0.05f});
        }

        // Forearm — parented to lower_arm, mid-length.
        pieces_.push_back({la,
            vec3(0, -0.13f, 0), quat(1, 0, 0, 0),
            vec3(0.14f, 0.25f, 0.14f), 3, 0.08f});

        // Gauntlet — parented to hand bone.
        const std::string hand = "hand" + side_suffix;
        pieces_.push_back({hand,
            vec3(0, -0.07f, 0.02f), quat(1, 0, 0, 0),
            vec3(0.17f, 0.14f, 0.15f), 0, 0.0f});

        // Claws — 5 small cones approximated as scaled boxes (spike-like).
        for (int j = 0; j < 5; ++j) {
            float offset = (j - 2) * 0.022f * xs;
            pieces_.push_back({hand,
                vec3(offset, -0.12f, 0.10f), quat(1, 0, 0, 0),
                vec3(0.012f, 0.060f, 0.012f), 0, -0.15f});
        }
    }

    // ========== HIP + LEGS ==========
    const vec3 root_rest(0, kRootY, 0);

    // 26. Hip belt — parented to 'root'.
    pieces_.push_back({"root",
        vec3(0, 0.86f, 0) - root_rest, quat(1, 0, 0, 0),
        vec3(0.54f, 0.14f, 0.41f), 0, 0.0f});

    // 27. Codpiece — small downward pyramid approximated as a tilted box.
    pieces_.push_back({"root",
        vec3(0, 0.74f, 0.12f) - root_rest,
        glm::angleAxis(glm::radians(180.0f), vec3(1, 0, 0)),
        vec3(0.18f, 0.18f, 0.10f), 0, 0.05f});

    for (int side_idx = 0; side_idx < 2; ++side_idx) {
        int xs = (side_idx == 0) ? -1 : 1;
        const std::string side_suffix = (xs < 0) ? "_l" : "_r";
        const std::string ul = "upper_leg" + side_suffix;
        const std::string ll = "lower_leg" + side_suffix;
        const std::string ft = "foot" + side_suffix;

        // upper_leg rest world = root + (±0.1, -0.4, 0) = (±0.1, 0.55, 0)
        // upper_leg bone origin is at the hip joint; bone extends down 0.40.
        // Thigh center is at local (0, -0.20, 0).

        // 28a. Thigh cylinder
        pieces_.push_back({ul,
            vec3(0, -0.20f, 0), quat(1, 0, 0, 0),
            vec3(0.20f, 0.40f, 0.20f), 3, 0.05f});
        // 28b. Front upper-leg plate
        pieces_.push_back({ul,
            vec3(0, -0.20f, 0.10f), quat(1, 0, 0, 0),
            vec3(0.14f, 0.26f, 0.04f), 0, -0.05f});

        // 29. Knee disks — parented to lower_leg (its origin is the knee joint).
        for (int i = 0; i < 3; ++i) {
            pieces_.push_back({ll,
                vec3(0, 0.0f - i * 0.025f, 0.02f), quat(1, 0, 0, 0),
                vec3(0.19f - i * 0.012f, 0.035f, 0.19f - i * 0.012f), 3, -0.05f});
        }

        // 30a. Shin cylinder
        pieces_.push_back({ll,
            vec3(0, -0.20f, 0), quat(1, 0, 0, 0),
            vec3(0.19f, 0.40f, 0.19f), 3, 0.05f});
        // 30b. Greave plate
        pieces_.push_back({ll,
            vec3(0, -0.20f, 0.09f), quat(1, 0, 0, 0),
            vec3(0.13f, 0.28f, 0.04f), 0, -0.05f});

        // 31. Boot — parented to foot bone.
        pieces_.push_back({ft,
            vec3(0, -0.06f, 0.04f), quat(1, 0, 0, 0),
            vec3(0.22f, 0.13f, 0.38f), 0, 0.0f});
        // Boot toe cap
        pieces_.push_back({ft,
            vec3(0, -0.02f, 0.18f), quat(1, 0, 0, 0),
            vec3(0.15f, 0.07f, 0.08f), 0, -0.10f});
    }
}

// ---------------------------------------------------------------------------
// compose_piece — bone world transform * piece local offset -> RenderEntity.
// ---------------------------------------------------------------------------
RenderEntity BerserkHaloCharacter::compose_piece(const ArmorPiece& p,
                                                 const vec4& color) const {
    RenderEntity e{};
    int32_t bone_idx = skeleton_.find_bone(p.bone);
    if (bone_idx < 0 || static_cast<size_t>(bone_idx) >= world_transforms_.size()) {
        return e;   // silently drop if bone missing
    }

    // Build local offset matrix.
    mat4 local = glm::translate(mat4(1.0f), p.local_offset) * glm::toMat4(p.local_rot);
    mat4 piece_world = world_transforms_[bone_idx] * local;

    // Decompose — we only need translation + rotation; scale is authored fixed.
    vec3 piece_pos(piece_world[3]);
    quat piece_rot = glm::quat_cast(piece_world);

    e.position  = piece_pos;
    e.rotation  = piece_rot;
    e.scale     = p.scale;
    e.color     = color;
    e.mesh_type = p.mesh_type;
    return e;
}

// ---------------------------------------------------------------------------
// update — evaluate animation, transform bones to world space, emit armor pieces.
// ---------------------------------------------------------------------------
void BerserkHaloCharacter::update(float dt, const vec3& world_pos, const quat& world_rot,
                                  float move_speed, float ground_height) {
    if (!initialized_) return;

    position_ = world_pos;
    rotation_ = world_rot;

    // Heavy walk is slower — don't scale as aggressively with move speed.
    float speed_factor = std::max(move_speed / 6.0f, 0.5f);
    auto local_transforms = anim_player_.evaluate(dt * speed_factor);
    world_transforms_ = anim::AnimationPlayer::compute_world_transforms(
        skeleton_, local_transforms);

    mat4 root_transform = glm::translate(mat4(1.0f), world_pos) * glm::toMat4(world_rot);
    if (ground_height != 0.0f) {
        root_transform[3][1] += ground_height;
    }
    for (auto& wt : world_transforms_) {
        wt = root_transform * wt;
    }

    renderables_.clear();
    renderables_.reserve(pieces_.size());

    // Visor is index 3 (the 4th piece) per build_piece_manifest.
    const size_t kVisorIdx = 3;
    for (size_t i = 0; i < pieces_.size(); ++i) {
        const auto& p = pieces_[i];
        vec4 c;
        if (i == kVisorIdx) {
            c = visor_color_;
        } else {
            vec3 rgb = shade(base_color_, p.shade_darken);
            // Damage flash: lerp toward red.
            if (hit_flash_ > 0.001f) {
                rgb = glm::mix(rgb, vec3(1.0f, 0.1f, 0.05f), hit_flash_);
            }
            c = vec4(rgb, 1.0f);
        }
        renderables_.push_back(compose_piece(p, c));
    }
}

void BerserkHaloCharacter::set_hit_flash(float flash01) {
    hit_flash_ = std::max(0.0f, std::min(flash01, 1.0f));
}

// ---------------------------------------------------------------------------
// get_attachment_transform — same semantics as HumanoidCharacter.
// ---------------------------------------------------------------------------
mat4 BerserkHaloCharacter::get_attachment_transform(const std::string& name) const {
    auto it = skeleton_.attachments.find(name);
    if (it == skeleton_.attachments.end()) {
        return mat4(1.0f);
    }
    const auto& attach = it->second;
    if (attach.bone_index >= world_transforms_.size()) {
        return mat4(1.0f);
    }
    mat4 local_offset = glm::translate(mat4(1.0f), attach.local_offset.position)
                      * glm::toMat4(attach.local_offset.rotation);
    return world_transforms_[attach.bone_index] * local_offset;
}

} // namespace odyssey
