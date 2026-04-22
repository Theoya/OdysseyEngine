#include "editor/prefab_ops.h"
#include "scene/prefab_loader.h"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

namespace odyssey::editor {

Result<std::filesystem::path, std::string> create_prefab_from_entity(
    const scene::EntityManager& em, EntityID id,
    const std::filesystem::path& prefabs_dir) {

    const auto* entity = em.get_entity(id);
    if (!entity) {
        return Result<std::filesystem::path, std::string>::err(
            "Entity " + std::to_string(id) + " not found");
    }

    // Generate a prefab filename from the entity name
    std::string safe_name = entity->name;
    // Simple sanitization: replace spaces with underscores
    for (char& c : safe_name) {
        if (c == ' ') c = '_';
    }
    if (safe_name.empty()) {
        safe_name = "entity_" + std::to_string(id);
    }

    std::filesystem::path prefab_path = prefabs_dir / (safe_name + ".prefab.xml");

    // Create the prefab XML
    pugi::xml_document doc;
    pugi::xml_node prefab_node = doc.append_child("prefab");

    // Copy entity attributes and components
    pugi::xml_node entity_node = prefab_node.append_child("entity");
    entity_node.append_attribute("id").set_value(std::to_string(id).c_str());
    entity_node.append_attribute("name").set_value(entity->name.c_str());
    entity_node.append_attribute("archetype").set_value(entity->archetype.c_str());

    // Copy transform if present
    {
        const auto& t = entity->components.transform;
        pugi::xml_node transform_node = entity_node.append_child("transform");
        // Position
        std::string pos_str = std::to_string(t.position.x) + " " +
                              std::to_string(t.position.y) + " " +
                              std::to_string(t.position.z);
        transform_node.append_attribute("position").set_value(pos_str.c_str());
        // Rotation (as quat)
        std::string rot_str = std::to_string(t.rotation.x) + " " +
                              std::to_string(t.rotation.y) + " " +
                              std::to_string(t.rotation.z) + " " +
                              std::to_string(t.rotation.w);
        transform_node.append_attribute("rotation").set_value(rot_str.c_str());
        // Scale
        std::string scale_str = std::to_string(t.scale.x) + " " +
                                std::to_string(t.scale.y) + " " +
                                std::to_string(t.scale.z);
        transform_node.append_attribute("scale").set_value(scale_str.c_str());
    }

    // Copy stats if present
    {
        const auto& s = entity->components.stats;
        pugi::xml_node stats_node = entity_node.append_child("stats");
        stats_node.append_attribute("health").set_value(s.health);
        stats_node.append_attribute("max_health").set_value(s.max_health);
    }

    // Write to file
    if (!doc.save_file(prefab_path.c_str())) {
        return Result<std::filesystem::path, std::string>::err(
            "Failed to write prefab file: " + prefab_path.string());
    }

    spdlog::info("[editor] Created prefab: {} (from entity {})", prefab_path.string(), id);
    return Result<std::filesystem::path, std::string>::ok(prefab_path);
}

Result<bool, std::string> open_prefab_in_isolation(
    const std::filesystem::path& prefab_path) {
    return Result<bool, std::string>::err("Prefab isolation mode not yet implemented in Batch H");
}

Result<bool, std::string> apply_prefab_overrides(
    const scene::EntityManager& em, EntityID id) {
    return Result<bool, std::string>::err("Prefab overrides not yet implemented in Batch H");
}

Result<bool, std::string> revert_prefab_overrides(
    scene::EntityManager& em, EntityID id) {
    return Result<bool, std::string>::err("Prefab overrides not yet implemented in Batch H");
}

Result<bool, std::string> unpack_prefab(scene::EntityManager& em, EntityID id) {
    auto* entity = em.get_entity(id);
    if (!entity) {
        return Result<bool, std::string>::err(
            "Entity " + std::to_string(id) + " not found");
    }

    // For Batch H, we simply delete the instance and return success.
    // Full unpack (re-instantiate children from prefab source) deferred to Batch I.
    em.destroy_entity(id);

    spdlog::info("[editor] Unpacked prefab instance {}", id);
    return Result<bool, std::string>::ok(true);
}

} // namespace odyssey::editor
