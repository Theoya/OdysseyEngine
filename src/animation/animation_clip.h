#pragma once
#include "core/types.h"
#include <string>
#include <vector>

namespace odyssey::anim {

struct BoneKeyframe {
    float time;
    vec3 position;
    quat rotation;
};

struct BoneTrack {
    uint32_t bone_index;
    std::vector<BoneKeyframe> keyframes;  // sorted by time
};

struct AnimationClip {
    std::string name;
    float duration;
    bool looping = false;
    std::vector<BoneTrack> tracks;
};

} // namespace odyssey::anim
