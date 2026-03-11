#pragma once
#include "animation/skeleton.h"
#include "animation/animation_clip.h"
#include "core/result.h"
#include <filesystem>

namespace odyssey::anim {

Result<Skeleton> load_skeleton(const std::filesystem::path& path);
Result<AnimationClip> load_animation_clip(const std::filesystem::path& path, const Skeleton& skeleton);

} // namespace odyssey::anim
