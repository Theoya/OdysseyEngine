#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace odyssey::anim {

struct Bone {
    std::string name;
    int32_t parent_index;       // -1 for root
    Transform rest_pose;        // local-space
    float length;               // for visualization
    float render_radius;        // cylinder thickness
};

struct Skeleton {
    std::string name;
    std::vector<Bone> bones;
    std::unordered_map<std::string, uint32_t> bone_index;

    struct AttachmentPoint {
        uint32_t bone_index;
        Transform local_offset;
    };
    std::unordered_map<std::string, AttachmentPoint> attachments;

    int32_t find_bone(const std::string& name) const {
        auto it = bone_index.find(name);
        return it != bone_index.end() ? static_cast<int32_t>(it->second) : -1;
    }
};

} // namespace odyssey::anim
