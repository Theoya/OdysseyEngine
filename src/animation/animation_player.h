#pragma once
#include "animation/skeleton.h"
#include "animation/animation_clip.h"
#include <vector>

namespace odyssey::anim {

class AnimationPlayer {
public:
    void set_skeleton(const Skeleton* skeleton);
    void play(const AnimationClip* clip, float speed = 1.0f, bool loop = true);
    void crossfade(const AnimationClip* clip, float duration = 0.2f);
    std::vector<Transform> evaluate(float dt);

    float current_time() const { return current_time_; }
    bool is_playing() const { return current_clip_ != nullptr; }

    // Pure static helpers
    static std::vector<mat4> compute_world_transforms(
        const Skeleton& skeleton, const std::vector<Transform>& local_transforms);
    static std::vector<Transform> sample_clip(
        const AnimationClip& clip, const Skeleton& skeleton, float time);
    static std::vector<Transform> blend_transforms(
        const std::vector<Transform>& a, const std::vector<Transform>& b, float t);

private:
    const Skeleton* skeleton_ = nullptr;
    const AnimationClip* current_clip_ = nullptr;
    const AnimationClip* crossfade_from_ = nullptr;
    float current_time_ = 0.0f;
    float crossfade_from_time_ = 0.0f;
    float speed_ = 1.0f;
    bool loop_ = true;
    float crossfade_duration_ = 0.0f;
    float crossfade_elapsed_ = 0.0f;
};

} // namespace odyssey::anim
