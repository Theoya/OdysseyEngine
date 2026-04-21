#include "scene/scene_loader.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <unordered_set>

namespace odyssey::scene {

// ---------------------------------------------------------------------------
// Phase 2: known-set tables for preserve-unknowns.
//
// The loader consumes these attributes + children explicitly. Anything else
// encountered on the same node is routed into the unknown-buckets on
// SceneData::EntityDesc / SceneData so the serializer can write them back.
// ---------------------------------------------------------------------------

static const std::unordered_set<std::string>& known_scene_attrs() {
    static const std::unordered_set<std::string> s{"name", "version"};
    return s;
}

static const std::unordered_set<std::string>& known_entity_attrs() {
    static const std::unordered_set<std::string> s{"id", "archetype", "count"};
    return s;
}

static const std::unordered_set<std::string>& known_entity_children() {
    static const std::unordered_set<std::string> s{
        "transform", "stats", "behavior", "mesh", "material",
        "script", "spawn_region", "pack"
    };
    return s;
}

// Serialize a pugi::xml_node to a raw XML string — used to snapshot unknown
// children without keeping pugi handles alive on SceneData.
static std::string node_to_string(const pugi::xml_node& node) {
    std::ostringstream oss;
    node.print(oss, "", pugi::format_raw);
    return oss.str();
}

vec3 parse_vec3(const std::string& str, vec3 default_val) {
    if (str.empty()) return default_val;

    std::istringstream iss(str);
    vec3 result = default_val;
    iss >> result.x >> result.y >> result.z;
    return result;
}

quat parse_quat(const std::string& str, quat default_val) {
    if (str.empty()) return default_val;

    std::istringstream iss(str);
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;
    iss >> x >> y >> z >> w;
    return quat{w, x, y, z};
}

static float parse_float(const char* str, float default_val) {
    if (!str || str[0] == '\0') return default_val;
    try {
        return std::stof(str);
    } catch (...) {
        return default_val;
    }
}

static uint32_t parse_uint(const char* str, uint32_t default_val) {
    if (!str || str[0] == '\0') return default_val;
    try {
        return static_cast<uint32_t>(std::stoul(str));
    } catch (...) {
        return default_val;
    }
}

// Populate an EntityDesc from a pugi <entity> node, capturing unknowns.
static void parse_entity_node(const pugi::xml_node& entity_node,
                              SceneData::EntityDesc& desc) {
    desc.id = entity_node.attribute("id").as_string("");
    desc.archetype = entity_node.attribute("archetype").as_string("");
    desc.count = entity_node.attribute("count").as_uint(1);

    // Capture unknown attributes in insertion order.
    const auto& known = known_entity_attrs();
    for (auto attr : entity_node.attributes()) {
        std::string name{attr.name()};
        if (known.find(name) == known.end()) {
            desc.unknown_attributes.emplace_back(std::move(name), attr.value());
        }
    }

    // Transform — attributes: position="x y z" rotation="x y z w" scale="x y z"
    auto transform_node = entity_node.child("transform");
    if (transform_node) {
        auto pos_attr = transform_node.attribute("position");
        if (pos_attr) {
            desc.transform.position = parse_vec3(pos_attr.as_string());
        }
        auto rot_attr = transform_node.attribute("rotation");
        if (rot_attr) {
            desc.transform.rotation = parse_quat(rot_attr.as_string());
        }
        auto scale_attr = transform_node.attribute("scale");
        if (scale_attr) {
            desc.transform.scale = parse_vec3(scale_attr.as_string(), vec3{1.f});
        }
    }

    // Stats — attributes: health, max_health, ammo, stamina, speed
    auto stats_node = entity_node.child("stats");
    if (stats_node) {
        desc.stats.health = stats_node.attribute("health").as_float(100.0f);
        desc.stats.max_health = stats_node.attribute("max_health").as_float(desc.stats.health);
        desc.stats.ammo = stats_node.attribute("ammo").as_float(0.0f);
        desc.stats.stamina = stats_node.attribute("stamina").as_float(100.0f);
        desc.stats.speed = stats_node.attribute("speed").as_float(5.0f);
    }

    // Behavior shader
    auto behavior_node = entity_node.child("behavior");
    if (behavior_node) {
        desc.behavior_shader = behavior_node.attribute("shader").as_string("");
    }

    // Mesh + material
    auto mesh_node = entity_node.child("mesh");
    if (mesh_node) {
        desc.mesh_src = mesh_node.attribute("src").as_string("");
    }
    auto material_node = entity_node.child("material");
    if (material_node) {
        desc.material_src = material_node.attribute("src").as_string("");
    }

    // Script
    auto script_node = entity_node.child("script");
    if (script_node) {
        desc.script_class = script_node.attribute("class").as_string("");
        desc.script_config = script_node.attribute("config").as_string("");
    }

    // Spawn region (as entity CHILD — shooter scene shape)
    auto spawn_node = entity_node.child("spawn_region");
    if (spawn_node) {
        desc.spawn_type = spawn_node.attribute("type").as_string("");
        auto center_attr = spawn_node.attribute("center");
        if (center_attr) {
            desc.spawn_center = parse_vec3(center_attr.as_string());
        }
        desc.spawn_radius = spawn_node.attribute("radius").as_float(0.0f);
    }

    // Pack info
    auto pack_node = entity_node.child("pack");
    if (pack_node) {
        desc.pack_leader = pack_node.attribute("leader").as_string("");
    }

    // Capture unknown element children.
    const auto& known_child = known_entity_children();
    for (auto child : entity_node.children()) {
        if (child.type() != pugi::node_element) continue;
        std::string n{child.name()};
        if (known_child.find(n) == known_child.end()) {
            desc.unknown_children_xml.push_back(node_to_string(child));
        }
    }
}

Result<SceneData> parse_scene_xml(const std::string& xml_content) {
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_string(xml_content.c_str());

    if (!parse_result) {
        return Result<SceneData>::err(
            std::string("Failed to parse scene XML: ") + parse_result.description());
    }

    auto scene_node = doc.child("scene");
    if (!scene_node) {
        return Result<SceneData>::err("Missing root <scene> element");
    }

    SceneData scene;
    scene.name = scene_node.attribute("name").as_string("unnamed");
    scene.version = scene_node.attribute("version").as_int(1);

    // Capture unknown scene-root attributes (e.g. lighting_profile, audio_bank).
    {
        const auto& known = known_scene_attrs();
        for (auto attr : scene_node.attributes()) {
            std::string name{attr.name()};
            if (known.find(name) == known.end()) {
                scene.unknown_scene_attributes.emplace_back(
                    std::move(name), attr.value());
            }
        }
    }

    // Parse world settings
    auto world_node = scene_node.child("world");
    if (world_node) {
        auto ts_node = world_node.child("time_scale");
        if (ts_node) {
            scene.time_scale = parse_float(ts_node.text().as_string(), 1.0f);
        }

        auto grav_node = world_node.child("gravity");
        if (grav_node) {
            scene.gravity = parse_vec3(grav_node.text().as_string(), vec3{0.0f, -9.81f, 0.0f});
        }
    }

    // Parse entities — <entity> nodes directly under <scene>.
    for (auto entity_node : scene_node.children("entity")) {
        SceneData::EntityDesc desc;
        parse_entity_node(entity_node, desc);
        scene.entities.push_back(std::move(desc));
    }

    // ALSO parse <entity> nodes nested inside <spawn_region> (showcase shape).
    // spawn_region wraps the batch-spawn metadata and the entity descriptor.
    for (auto sr_node : scene_node.children("spawn_region")) {
        for (auto entity_node : sr_node.children("entity")) {
            SceneData::EntityDesc desc;
            parse_entity_node(entity_node, desc);
            // Inherit spawn metadata from the surrounding <spawn_region>.
            // showcase uses attributes: position="x y z" radius="r"
            if (auto pos = sr_node.attribute("position")) {
                desc.spawn_center = parse_vec3(pos.as_string());
                if (desc.spawn_type.empty()) desc.spawn_type = "circle";
            }
            if (auto rad = sr_node.attribute("radius")) {
                desc.spawn_radius = rad.as_float(desc.spawn_radius);
            }
            scene.entities.push_back(std::move(desc));
        }
    }

    spdlog::info("Parsed scene '{}' with {} entity descriptors",
                 scene.name, scene.entities.size());
    return Result<SceneData>::ok(std::move(scene));
}

Result<SceneData> load_scene_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<SceneData>::err("Scene file not found: " + path.string());
    }

    // Read in BINARY mode so CRLF is preserved — byte-identical round-trip
    // depends on not silently normalizing line endings on Windows.
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return Result<SceneData>::err("Failed to open scene file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    spdlog::info("Loading scene from '{}'", path.string());
    auto result = parse_scene_xml(content);
    if (result.is_ok()) {
        // Snapshot the raw source for scene_serializer's preserve-bytes path.
        auto data = std::move(result).value();
        data.preserved_source = std::move(content);
        data.mutated = false;
        return Result<SceneData>::ok(std::move(data));
    }
    return result;
}

void populate_entities(EntityManager& manager, const SceneData& scene) {
    for (const auto& desc : scene.entities) {
        if (desc.count <= 1) {
            // Single entity
            std::string name = desc.id.empty() ? desc.archetype : desc.id;
            EntityID id = manager.create_entity(name, desc.archetype);

            Entity* entity = manager.get_entity(id);
            if (entity) {
                entity->components.transform = desc.transform;
                entity->components.stats = desc.stats;
                entity->components.behavior_shader = desc.behavior_shader;
                entity->components.mesh_path = desc.mesh_src;
                entity->components.material_path = desc.material_src;
                entity->components.script_class = desc.script_class;
                entity->components.script_config = desc.script_config;
            }
        } else {
            // Batch spawn
            std::string prefix = desc.id.empty() ? desc.archetype : desc.id;
            auto ids = manager.create_entities(desc.archetype, desc.count, prefix);

            for (size_t i = 0; i < ids.size(); ++i) {
                Entity* entity = manager.get_entity(ids[i]);
                if (!entity) continue;

                entity->components.transform = desc.transform;
                entity->components.stats = desc.stats;
                entity->components.behavior_shader = desc.behavior_shader;
                entity->components.mesh_path = desc.mesh_src;
                entity->components.material_path = desc.material_src;
                entity->components.script_class = desc.script_class;
                entity->components.script_config = desc.script_config;

                // Offset positions for batch spawning within the spawn region
                if (!desc.spawn_type.empty() && desc.spawn_radius > 0.0f) {
                    float angle = (2.0f * 3.14159265f * static_cast<float>(i))
                                  / static_cast<float>(desc.count);
                    float radius_frac = desc.spawn_radius
                                        * (static_cast<float>(i) + 1.0f)
                                        / static_cast<float>(desc.count);

                    if (desc.spawn_type == "circle") {
                        entity->components.transform.position = desc.spawn_center
                            + vec3{std::cos(angle) * radius_frac,
                                   0.0f,
                                   std::sin(angle) * radius_frac};
                    } else {
                        // Default: distribute linearly along x
                        float offset = desc.spawn_radius * 2.0f
                                        * (static_cast<float>(i) / static_cast<float>(desc.count))
                                        - desc.spawn_radius;
                        entity->components.transform.position = desc.spawn_center
                            + vec3{offset, 0.0f, 0.0f};
                    }
                }
            }
        }
    }

    spdlog::info("Populated {} entities from scene data", manager.entity_count());
}

std::vector<std::filesystem::path> find_scene_files(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> results;

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        spdlog::warn("Scene directory does not exist: {}", dir.string());
        return results;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        // Match *.scene.xml
        if (filename.size() > 10 && filename.substr(filename.size() - 10) == ".scene.xml") {
            results.push_back(entry.path());
        }
    }

    spdlog::debug("Found {} scene files in '{}'", results.size(), dir.string());
    return results;
}

} // namespace odyssey::scene
