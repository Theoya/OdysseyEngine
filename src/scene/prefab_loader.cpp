#include "scene/prefab_loader.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace odyssey::scene {

// Forward-declare parse helpers from scene_loader
vec3 parse_vec3(const std::string& str, vec3 default_val);
quat parse_quat(const std::string& str, quat default_val);

static float parse_float(const char* str, float default_val) {
    if (!str || str[0] == '\0') return default_val;
    try {
        return std::stof(str);
    } catch (...) {
        return default_val;
    }
}

Result<PrefabData> parse_prefab_xml(const std::string& xml_content) {
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_string(xml_content.c_str());

    if (!parse_result) {
        return Result<PrefabData>::err(
            std::string("Failed to parse prefab XML: ") + parse_result.description());
    }

    auto prefab_node = doc.child("prefab");
    if (!prefab_node) {
        return Result<PrefabData>::err("Missing root <prefab> element");
    }

    PrefabData prefab;
    prefab.name = prefab_node.attribute("name").as_string("unnamed");
    prefab.version = prefab_node.attribute("version").as_int(1);

    // Default components
    auto& comps = prefab.default_components;

    // Transform
    auto transform_node = prefab_node.child("transform");
    if (transform_node) {
        auto pos_node = transform_node.child("position");
        if (pos_node) {
            comps.transform.position = parse_vec3(pos_node.text().as_string(), vec3{0.f});
        }
        auto rot_node = transform_node.child("rotation");
        if (rot_node) {
            comps.transform.rotation = parse_quat(rot_node.text().as_string(),
                                                   quat{1.f, 0.f, 0.f, 0.f});
        }
        auto scale_node = transform_node.child("scale");
        if (scale_node) {
            comps.transform.scale = parse_vec3(scale_node.text().as_string(), vec3{1.f});
        }
    }

    // Stats
    auto stats_node = prefab_node.child("stats");
    if (stats_node) {
        comps.stats.health = parse_float(
            stats_node.child("health").text().as_string(), 100.0f);
        comps.stats.max_health = parse_float(
            stats_node.child("max_health").text().as_string(), comps.stats.health);
        comps.stats.ammo = parse_float(
            stats_node.child("ammo").text().as_string(), 0.0f);
        comps.stats.stamina = parse_float(
            stats_node.child("stamina").text().as_string(), 100.0f);
        comps.stats.speed = parse_float(
            stats_node.child("speed").text().as_string(), 5.0f);
    }

    // Behavior
    auto behavior_node = prefab_node.child("behavior");
    if (behavior_node) {
        comps.behavior_shader = behavior_node.attribute("shader").as_string("");
    }

    // Mesh
    auto mesh_node = prefab_node.child("mesh");
    if (mesh_node) {
        comps.mesh_path = mesh_node.attribute("src").as_string("");
    }

    // Material
    auto material_node = prefab_node.child("material");
    if (material_node) {
        comps.material_path = material_node.attribute("src").as_string("");
    }

    // Script
    auto script_node = prefab_node.child("script");
    if (script_node) {
        comps.script_class = script_node.attribute("class").as_string("");
        comps.script_config = script_node.attribute("config").as_string("");
    }

    // Collider
    auto collider_node = prefab_node.child("collider");
    if (collider_node) {
        prefab.collider_type = collider_node.attribute("type").as_string("");
        prefab.collider_radius = collider_node.attribute("radius").as_float(0.5f);
        prefab.collider_height = collider_node.attribute("height").as_float(1.8f);
        prefab.collider_auto_fit = collider_node.attribute("auto_fit").as_bool(false);
    }

    // AI
    auto ai_node = prefab_node.child("ai");
    if (ai_node) {
        prefab.initial_ai_state = ai_node.attribute("initial_state").as_string("");
    }

    spdlog::info("Parsed prefab '{}'", prefab.name);
    return Result<PrefabData>::ok(std::move(prefab));
}

Result<PrefabData> load_prefab_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<PrefabData>::err("Prefab file not found: " + path.string());
    }

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        return Result<PrefabData>::err("Failed to open prefab file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    spdlog::info("Loading prefab from '{}'", path.string());
    return parse_prefab_xml(content);
}

// PrefabRegistry implementation

Result<const PrefabData*> PrefabRegistry::load(const std::filesystem::path& path) {
    auto result = load_prefab_file(path);
    if (result.is_err()) {
        return Result<const PrefabData*>::err(result.error());
    }

    PrefabData prefab = std::move(result).value();
    std::string name = prefab.name;
    prefabs_[name] = std::move(prefab);

    spdlog::info("Registered prefab '{}'", name);
    return Result<const PrefabData*>::ok(&prefabs_[name]);
}

const PrefabData* PrefabRegistry::get(const std::string& name) const {
    auto it = prefabs_.find(name);
    if (it == prefabs_.end()) return nullptr;
    return &it->second;
}

void PrefabRegistry::apply_prefab(Entity& entity, const PrefabData& prefab) const {
    entity.components.transform = prefab.default_components.transform;
    entity.components.stats = prefab.default_components.stats;
    entity.components.prefab_source = prefab.name;

    // Only set non-empty fields from the prefab
    if (!prefab.default_components.behavior_shader.empty()) {
        entity.components.behavior_shader = prefab.default_components.behavior_shader;
    }
    if (!prefab.default_components.mesh_path.empty()) {
        entity.components.mesh_path = prefab.default_components.mesh_path;
    }
    if (!prefab.default_components.material_path.empty()) {
        entity.components.material_path = prefab.default_components.material_path;
    }
    if (!prefab.default_components.script_class.empty()) {
        entity.components.script_class = prefab.default_components.script_class;
    }
    if (!prefab.default_components.script_config.empty()) {
        entity.components.script_config = prefab.default_components.script_config;
    }

    spdlog::debug("Applied prefab '{}' to entity '{}'", prefab.name, entity.name);
}

void PrefabRegistry::clear() {
    prefabs_.clear();
    spdlog::debug("PrefabRegistry cleared");
}

} // namespace odyssey::scene
