#pragma once
#include "core/types.h"
#include "app/game.h"
#include "animation/skeleton.h"
#include "animation/animation_clip.h"
#include "animation/animation_player.h"
#include "animation/skeleton_renderer.h"
#include <vector>
#include <string>
#include <filesystem>

namespace odyssey {

// ---------------------------------------------------------------------------
// BerserkHaloCharacter — heavy-armored humanoid variant.
//
// Shares the 19-bone humanoid skeleton with HumanoidCharacter but:
//   1. Loads a heavier walk cycle (demo/showcase/assets/berserk_halo_mk3/
//      berserk_halo_walk.anim.xml) alongside the stock idle.
//   2. Emits RenderEntity primitives for each armor piece, parented to the
//      appropriate bone's WORLD transform — i.e. rigid-bone-follow, not skinned.
//   3. Suppresses the stick-figure armature cylinders so we see armor, not bones.
//      (The armor primitive pieces visually replace the bones they cover.)
//
// This is the "parent-to-bone rigid chunks" fallback documented in the model
// task brief — true skinning isn't wired in the renderer (RenderEntity::mesh_type
// is {0=box,1=sphere,2=ground,3=cylinder} only), so we build the armor out of
// those same primitives, placing each rigidly at a bone.
// ---------------------------------------------------------------------------
class BerserkHaloCharacter {
public:
    // Load humanoid skeleton + idle from humanoid_assets_dir (the normal
    // fps_humanoid assets dir) and load the heavy walk from berserk_assets_dir.
    Result<bool> initialize(const std::filesystem::path& humanoid_assets_dir,
                            const std::filesystem::path& berserk_assets_dir);

    void update(float dt, const vec3& world_pos, const quat& world_rot,
                float move_speed, float ground_height);

    const std::vector<RenderEntity>& get_renderables() const { return renderables_; }

    // Damage flash — tints all armor pieces.
    void set_hit_flash(float flash01);
    // Whole-armor base color (defaults to near-black matte).
    void set_base_color(const vec4& color) { base_color_ = color; }

    mat4 get_attachment_transform(const std::string& name) const;

    // Animation control
    void play_walk() { if (walk_clip_.duration > 0) anim_player_.play(&walk_clip_, 1.0f, true); }
    void play_idle() { if (idle_clip_.duration > 0) anim_player_.play(&idle_clip_, 1.0f, true); }
    void crossfade_to_walk(float dur = 0.3f) { if (walk_clip_.duration > 0) anim_player_.crossfade(&walk_clip_, dur); }
    void crossfade_to_idle(float dur = 0.3f) { if (idle_clip_.duration > 0) anim_player_.crossfade(&idle_clip_, dur); }

private:
    // Each piece is a primitive placed in a bone's LOCAL space
    // (post-root-transform, same frame the bone world_transform is in).
    struct ArmorPiece {
        std::string bone;
        vec3 local_offset;   // translation in bone-local space
        quat local_rot;      // rotation in bone-local space
        vec3 scale;          // scale applied to the primitive
        int  mesh_type;      // 0=box, 1=sphere, 3=cylinder
        float shade_darken;  // 0..1 multiplier so deep shadows read
    };

    anim::Skeleton skeleton_;
    anim::AnimationClip walk_clip_;
    anim::AnimationClip idle_clip_;
    anim::AnimationPlayer anim_player_;

    std::vector<RenderEntity> renderables_;
    std::vector<mat4> world_transforms_;
    std::vector<ArmorPiece> pieces_;

    vec3 position_{0.0f};
    quat rotation_{1.0f, 0.0f, 0.0f, 0.0f};

    vec4 base_color_{0.07f, 0.07f, 0.08f, 1.0f};   // near-black matte
    vec4 edge_hilite_{0.80f, 0.80f, 0.82f, 1.0f};  // near-white edge read
    vec4 visor_color_{1.00f, 0.95f, 0.55f, 1.0f};  // glowing-white (slight warm)
    float hit_flash_ = 0.0f;

    bool initialized_ = false;

    // Populate pieces_ with the ~30 armor primitive placements.
    void build_piece_manifest();
    // Compose a RenderEntity by sampling bone world transform and mixing piece offsets.
    RenderEntity compose_piece(const ArmorPiece& p, const vec4& color) const;
};

} // namespace odyssey
