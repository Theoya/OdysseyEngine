#include "animation/skeleton_loader.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

namespace odyssey::anim {

namespace {

vec3 parse_vec3(const char* str) {
    vec3 v(0.0f);
    if (!str) return v;
    std::istringstream ss(str);
    ss >> v.x >> v.y >> v.z;
    return v;
}

quat parse_quat(const char* str) {
    // Expected format: "x y z w"
    quat q(1.0f, 0.0f, 0.0f, 0.0f); // identity (w,x,y,z)
    if (!str) return q;
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    std::istringstream ss(str);
    ss >> x >> y >> z >> w;
    return quat(w, x, y, z); // glm::quat constructor is (w, x, y, z)
}

} // anonymous namespace

Result<Skeleton> load_skeleton(const std::filesystem::path& path) {
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_file(path.c_str());

    if (!parse_result) {
        return Result<Skeleton>::err(
            "Failed to parse skeleton XML '" + path.string() + "': " + parse_result.description());
    }

    auto skeleton_node = doc.child("skeleton");
    if (!skeleton_node) {
        return Result<Skeleton>::err("Missing <skeleton> root element in '" + path.string() + "'");
    }

    Skeleton skeleton;
    skeleton.name = skeleton_node.attribute("name").as_string("unnamed");

    // First pass: collect all bone names for parent lookup
    // We process bones in document order, which must be topologically sorted
    // (parents before children)
    std::unordered_map<std::string, uint32_t> name_to_index;

    for (auto bone_node : skeleton_node.children("bone")) {
        Bone bone;
        bone.name = bone_node.attribute("name").as_string("");

        if (bone.name.empty()) {
            spdlog::warn("Skeleton '{}': skipping bone with empty name", skeleton.name);
            continue;
        }

        // Parent lookup
        std::string parent_name = bone_node.attribute("parent").as_string("");
        if (parent_name.empty()) {
            bone.parent_index = -1;
        } else {
            auto it = name_to_index.find(parent_name);
            if (it != name_to_index.end()) {
                bone.parent_index = static_cast<int32_t>(it->second);
            } else {
                spdlog::warn("Skeleton '{}': bone '{}' references unknown parent '{}'",
                    skeleton.name, bone.name, parent_name);
                bone.parent_index = -1;
            }
        }

        // Rest pose
        bone.rest_pose.position = parse_vec3(bone_node.attribute("position").as_string("0 0 0"));
        bone.rest_pose.rotation = parse_quat(bone_node.attribute("rotation").as_string("0 0 0 1"));
        bone.rest_pose.scale = vec3(1.0f);

        // Visualization
        bone.length = bone_node.attribute("length").as_float(0.1f);
        bone.render_radius = bone_node.attribute("radius").as_float(0.02f);

        uint32_t index = static_cast<uint32_t>(skeleton.bones.size());
        name_to_index[bone.name] = index;
        skeleton.bones.push_back(std::move(bone));
    }

    skeleton.bone_index = std::move(name_to_index);

    // Parse attachments
    for (auto attach_node : skeleton_node.children("attachment")) {
        std::string attach_name = attach_node.attribute("name").as_string("");
        std::string bone_name = attach_node.attribute("bone").as_string("");

        if (attach_name.empty() || bone_name.empty()) {
            spdlog::warn("Skeleton '{}': skipping attachment with empty name or bone", skeleton.name);
            continue;
        }

        auto bone_it = skeleton.bone_index.find(bone_name);
        if (bone_it == skeleton.bone_index.end()) {
            spdlog::warn("Skeleton '{}': attachment '{}' references unknown bone '{}'",
                skeleton.name, attach_name, bone_name);
            continue;
        }

        Skeleton::AttachmentPoint attachment;
        attachment.bone_index = bone_it->second;
        attachment.local_offset.position = parse_vec3(attach_node.attribute("position").as_string("0 0 0"));
        attachment.local_offset.rotation = parse_quat(attach_node.attribute("rotation").as_string("0 0 0 1"));
        attachment.local_offset.scale = vec3(1.0f);

        skeleton.attachments[attach_name] = attachment;
    }

    spdlog::info("Loaded skeleton '{}' with {} bones, {} attachments",
        skeleton.name, skeleton.bones.size(), skeleton.attachments.size());

    return Result<Skeleton>::ok(std::move(skeleton));
}

Result<AnimationClip> load_animation_clip(const std::filesystem::path& path, const Skeleton& skeleton) {
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_file(path.c_str());

    if (!parse_result) {
        return Result<AnimationClip>::err(
            "Failed to parse animation XML '" + path.string() + "': " + parse_result.description());
    }

    auto anim_node = doc.child("animation");
    if (!anim_node) {
        return Result<AnimationClip>::err("Missing <animation> root element in '" + path.string() + "'");
    }

    AnimationClip clip;
    clip.name = anim_node.attribute("name").as_string("unnamed");
    clip.duration = anim_node.attribute("duration").as_float(1.0f);
    clip.looping = anim_node.attribute("looping").as_bool(false);

    for (auto track_node : anim_node.children("track")) {
        std::string bone_name = track_node.attribute("bone").as_string("");

        if (bone_name.empty()) {
            spdlog::warn("Animation '{}': skipping track with empty bone name", clip.name);
            continue;
        }

        int32_t bone_idx = skeleton.find_bone(bone_name);
        if (bone_idx < 0) {
            spdlog::warn("Animation '{}': track references unknown bone '{}', skipping",
                clip.name, bone_name);
            continue;
        }

        BoneTrack track;
        track.bone_index = static_cast<uint32_t>(bone_idx);

        for (auto key_node : track_node.children("key")) {
            BoneKeyframe kf;
            kf.time = key_node.attribute("time").as_float(0.0f);
            kf.position = parse_vec3(key_node.attribute("position").as_string("0 0 0"));
            kf.rotation = parse_quat(key_node.attribute("rotation").as_string("0 0 0 1"));
            track.keyframes.push_back(kf);
        }

        // Ensure keyframes are sorted by time
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const BoneKeyframe& a, const BoneKeyframe& b) { return a.time < b.time; });

        if (!track.keyframes.empty()) {
            clip.tracks.push_back(std::move(track));
        }
    }

    spdlog::info("Loaded animation '{}': duration={:.2f}s, {} tracks, looping={}",
        clip.name, clip.duration, clip.tracks.size(), clip.looping);

    return Result<AnimationClip>::ok(std::move(clip));
}

} // namespace odyssey::anim
