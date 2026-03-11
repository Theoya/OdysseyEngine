#include "animation/animation_player.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace odyssey::anim {

void AnimationPlayer::set_skeleton(const Skeleton* skeleton) {
    skeleton_ = skeleton;
}

void AnimationPlayer::play(const AnimationClip* clip, float speed, bool loop) {
    current_clip_ = clip;
    current_time_ = 0.0f;
    speed_ = speed;
    loop_ = loop;
    crossfade_from_ = nullptr;
    crossfade_duration_ = 0.0f;
    crossfade_elapsed_ = 0.0f;
}

void AnimationPlayer::crossfade(const AnimationClip* clip, float duration) {
    if (!skeleton_ || !clip) return;

    crossfade_from_ = current_clip_;
    crossfade_from_time_ = current_time_;
    current_clip_ = clip;
    current_time_ = 0.0f;
    crossfade_duration_ = duration;
    crossfade_elapsed_ = 0.0f;
}

std::vector<Transform> AnimationPlayer::evaluate(float dt) {
    if (!skeleton_ || !current_clip_) {
        // Return rest pose if nothing is playing
        if (skeleton_) {
            std::vector<Transform> rest(skeleton_->bones.size());
            for (size_t i = 0; i < skeleton_->bones.size(); ++i) {
                rest[i] = skeleton_->bones[i].rest_pose;
            }
            return rest;
        }
        return {};
    }

    // Advance current clip time
    current_time_ += dt * speed_;
    if (loop_ && current_clip_->duration > 0.0f) {
        current_time_ = std::fmod(current_time_, current_clip_->duration);
        if (current_time_ < 0.0f) current_time_ += current_clip_->duration;
    } else {
        current_time_ = std::clamp(current_time_, 0.0f, current_clip_->duration);
    }

    // Sample current clip
    auto current_pose = sample_clip(*current_clip_, *skeleton_, current_time_);

    // Handle crossfade blending
    if (crossfade_from_ && crossfade_duration_ > 0.0f) {
        crossfade_elapsed_ += dt;
        float blend_t = std::clamp(crossfade_elapsed_ / crossfade_duration_, 0.0f, 1.0f);

        // Sample the "from" clip
        crossfade_from_time_ += dt * speed_;
        if (crossfade_from_->looping && crossfade_from_->duration > 0.0f) {
            crossfade_from_time_ = std::fmod(crossfade_from_time_, crossfade_from_->duration);
            if (crossfade_from_time_ < 0.0f) crossfade_from_time_ += crossfade_from_->duration;
        } else {
            crossfade_from_time_ = std::clamp(crossfade_from_time_, 0.0f, crossfade_from_->duration);
        }

        auto from_pose = sample_clip(*crossfade_from_, *skeleton_, crossfade_from_time_);
        current_pose = blend_transforms(from_pose, current_pose, blend_t);

        // End crossfade when complete
        if (crossfade_elapsed_ >= crossfade_duration_) {
            crossfade_from_ = nullptr;
            crossfade_duration_ = 0.0f;
            crossfade_elapsed_ = 0.0f;
        }
    }

    return current_pose;
}

std::vector<Transform> AnimationPlayer::sample_clip(
    const AnimationClip& clip, const Skeleton& skeleton, float time) {

    std::vector<Transform> result(skeleton.bones.size());

    // Initialize to rest pose
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        result[i] = skeleton.bones[i].rest_pose;
    }

    // Overlay tracks
    for (const auto& track : clip.tracks) {
        if (track.bone_index >= skeleton.bones.size()) continue;
        if (track.keyframes.empty()) continue;

        const auto& keys = track.keyframes;

        // Clamp time to first/last keyframe
        if (time <= keys.front().time) {
            result[track.bone_index].position = keys.front().position;
            result[track.bone_index].rotation = keys.front().rotation;
            continue;
        }
        if (time >= keys.back().time) {
            result[track.bone_index].position = keys.back().position;
            result[track.bone_index].rotation = keys.back().rotation;
            continue;
        }

        // Binary search for the bracket keyframes
        auto it = std::lower_bound(keys.begin(), keys.end(), time,
            [](const BoneKeyframe& kf, float t) { return kf.time < t; });

        // it points to the first keyframe with time >= time
        size_t idx_b = static_cast<size_t>(it - keys.begin());
        size_t idx_a = idx_b > 0 ? idx_b - 1 : 0;

        const auto& ka = keys[idx_a];
        const auto& kb = keys[idx_b];

        float segment_duration = kb.time - ka.time;
        float t = (segment_duration > 0.0f) ? (time - ka.time) / segment_duration : 0.0f;

        result[track.bone_index].position = glm::mix(ka.position, kb.position, t);
        result[track.bone_index].rotation = glm::slerp(ka.rotation, kb.rotation, t);
    }

    return result;
}

std::vector<Transform> AnimationPlayer::blend_transforms(
    const std::vector<Transform>& a, const std::vector<Transform>& b, float t) {

    size_t count = std::min(a.size(), b.size());
    std::vector<Transform> result(count);

    for (size_t i = 0; i < count; ++i) {
        result[i].position = glm::mix(a[i].position, b[i].position, t);
        result[i].rotation = glm::slerp(a[i].rotation, b[i].rotation, t);
        result[i].scale = glm::mix(a[i].scale, b[i].scale, t);
    }

    return result;
}

std::vector<mat4> AnimationPlayer::compute_world_transforms(
    const Skeleton& skeleton, const std::vector<Transform>& local_transforms) {

    size_t bone_count = skeleton.bones.size();
    std::vector<mat4> world(bone_count, mat4(1.0f));

    for (size_t i = 0; i < bone_count; ++i) {
        // Build local matrix from transform
        mat4 local_mat = glm::translate(mat4(1.0f), local_transforms[i].position);
        local_mat *= glm::mat4_cast(local_transforms[i].rotation);
        local_mat = glm::scale(local_mat, local_transforms[i].scale);

        int32_t parent = skeleton.bones[i].parent_index;
        if (parent >= 0 && static_cast<size_t>(parent) < bone_count) {
            world[i] = world[static_cast<size_t>(parent)] * local_mat;
        } else {
            world[i] = local_mat;
        }
    }

    return world;
}

} // namespace odyssey::anim
