#pragma once
#include "core/types.h"
#include "app/game.h"
#include "animation/skeleton.h"
#include "animation/animation_clip.h"
#include "animation/animation_player.h"
#include "animation/skeleton_renderer.h"
#include <vector>
#include <filesystem>

namespace odyssey {

class HumanoidCharacter {
public:
    Result<bool> initialize(const std::filesystem::path& assets_dir);

    void update(float dt, const vec3& world_pos, const quat& world_rot,
                float move_speed, float ground_height);

    const std::vector<RenderEntity>& get_renderables() const { return renderables_; }

    void set_color(const vec4& color) { config_.bone_color = color; }
    void set_gun_visible(bool visible) { gun_visible_ = visible; }

    mat4 get_attachment_transform(const std::string& name) const;

    // Animation control
    void play_walk() { if (walk_clip_.duration > 0) anim_player_.play(&walk_clip_, 1.0f, true); }
    void play_idle() { if (idle_clip_.duration > 0) anim_player_.play(&idle_clip_, 1.0f, true); }
    void crossfade_to_walk(float dur = 0.2f) { if (walk_clip_.duration > 0) anim_player_.crossfade(&walk_clip_, dur); }
    void crossfade_to_idle(float dur = 0.2f) { if (idle_clip_.duration > 0) anim_player_.crossfade(&idle_clip_, dur); }

private:
    anim::Skeleton skeleton_;
    anim::AnimationClip walk_clip_;
    anim::AnimationClip idle_clip_;
    anim::AnimationPlayer anim_player_;
    anim::SkeletonRenderConfig config_;

    std::vector<RenderEntity> renderables_;
    std::vector<mat4> world_transforms_;

    vec3 position_{0.0f};
    quat rotation_{1.0f, 0.0f, 0.0f, 0.0f};

    bool gun_visible_ = true;
    bool initialized_ = false;

    void build_gun_renderables();
};

} // namespace odyssey
