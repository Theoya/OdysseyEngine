#pragma once
#include "core/types.h"
#include "core/result.h"
#include "scene/entity_manager.h"
#include <string>
#include <filesystem>
#include <utility>
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

        // Proximity-voice audibility radius in meters, parsed from the
        // `voice_range` attribute on <stats>. Default 25.0f = d_max from the
        // inverse-amplitude attenuation curve authored in
        // docs/design/proximity_chat_audio.md. Authored-time bound is
        // [0, 50]; server hard-clamps ingress to 50m at runtime
        // (docs/decisions/2026-04-20-proximity-voice-chat.md).
        //
        // Intentionally NOT a member of `EntityStats` because that struct is
        // uploaded byte-for-byte to the Nadir GPU SSBO (EntityStatsGPU) and
        // adding a 6th float would require a matching shader-side layout
        // bump. Voice is a CPU-side audio/net concern, so it rides on
        // EntityDesc instead.
        float voice_range = 25.0f;

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

        // --- Phase 2: preserve-unknowns ---
        // Attributes on <entity> that are NOT in the known-fields set above.
        // Stored in original insertion order. Examples from showcase:
        //   (material_override, "demo/showcase/materials/gold_leaf.mat.xml")
        //   (light_type, "directional"), (kelvin, "5500"), (intensity, "3.0")
        //   (music_state_machine, "demo/showcase/music/showcase.music.xml")
        std::vector<std::pair<std::string, std::string>> unknown_attributes;

        // Child elements of <entity> that the loader did not consume.
        // Snapshotted as raw serialized XML strings (NOT pugi handles)
        // so SceneData has stable lifetime independent of pugi's arena.
        // Rare in the current showcase but allowed by the contract.
        std::vector<std::string> unknown_children_xml;
    };

    std::vector<EntityDesc> entities;

    // --- Phase 2: preserve-unknowns (scene level) ---
    // Unknown attributes on the <scene> root. Example from showcase:
    //   lighting_profile="liminal", audio_bank="showcase_bank"
    std::vector<std::pair<std::string, std::string>> unknown_scene_attributes;

    // Raw UTF-8 contents of the source file, captured at load time.
    // The Phase 2 serializer writes this back verbatim when `mutated` is
    // false — guaranteeing byte-identical round-trip without needing a
    // full programmatic reconstruction path.
    // Empty for SceneData built in-memory (tests, procedural scenes).
    std::string preserved_source;

    // Set to true by any API that modifies SceneData after load. Serializer
    // honors this: unmutated → echo preserved_source, mutated → reconstruct.
    // Phase 2 Inspector remains read-only so `mutated` stays false in the
    // editor's main flow; explicit tests exercise both paths.
    bool mutated = false;
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
