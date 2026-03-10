#pragma once
#include "core/types.h"
#include "core/result.h"
#include "scene/entity_manager.h"
#include <string>
#include <filesystem>
#include <vector>

namespace odyssey::scene {

// Parsed scene data (pure data, no side effects)
struct SceneData {
    std::string name;
    int version = 1;

    // World settings
    float time_scale = 1.0f;
    vec3 gravity{0.0f, -9.81f, 0.0f};

    // Entity descriptors (parsed from XML)
    struct EntityDesc {
        std::string id;
        std::string archetype;
        uint32_t count = 1;  // for batch spawning
        Transform transform;
        EntityStats stats;
        std::string behavior_shader;
        std::string mesh_src;
        std::string material_src;
        std::string script_class;
        std::string script_config;

        // Spawn region (for count > 1)
        std::string spawn_type;  // "circle", "box", etc.
        vec3 spawn_center{0.f};
        float spawn_radius = 0.0f;

        // Pack info
        std::string pack_leader;
    };

    std::vector<EntityDesc> entities;
};

// Pure: parse scene XML string to SceneData
Result<SceneData> parse_scene_xml(const std::string& xml_content);

// Pure: parse a vec3 from space-separated string "x y z"
vec3 parse_vec3(const std::string& str, vec3 default_val = vec3{0.f});

// Pure: parse a quat from space-separated string "x y z w"
quat parse_quat(const std::string& str, quat default_val = quat{1.f, 0.f, 0.f, 0.f});

// Impure: load scene from file path
Result<SceneData> load_scene_file(const std::filesystem::path& path);

// Populate an EntityManager from parsed scene data
void populate_entities(EntityManager& manager, const SceneData& scene);

// List all .scene.xml files in a directory
std::vector<std::filesystem::path> find_scene_files(const std::filesystem::path& dir);

} // namespace odyssey::scene
