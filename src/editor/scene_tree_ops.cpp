#include "editor/scene_tree_ops.h"

#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>

namespace odyssey::editor {

bool matches_filter(const scene::Entity& entity, const std::string& filter) {
    if (filter.empty()) return true;

    // Check name (case-insensitive substring).
    auto ci_contains = [](const std::string& hay, const std::string& needle) {
        if (needle.empty()) return true;
        if (needle.size() > hay.size()) return false;
        auto it = std::search(
            hay.begin(), hay.end(),
            needle.begin(), needle.end(),
            [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            });
        return it != hay.end();
    };

    if (ci_contains(entity.name, filter)) return true;
    if (ci_contains(entity.archetype, filter)) return true;

    return false;
}

Result<EntityID, std::string> duplicate_entity(
    scene::EntityManager& em,
    EntityID src_id) {

    const scene::Entity* src = em.get_entity(src_id);
    if (!src) {
        return Result<EntityID, std::string>::err("Entity " + std::to_string(src_id) + " not found");
    }

    // Create new entity with same archetype and "[src_name] (Copy)" name.
    std::string new_name = src->name + " (Copy)";
    EntityID new_id = em.create_entity(new_name, src->archetype);

    // Copy all components from source to new entity.
    scene::Entity* new_entity = em.get_entity(new_id);
    if (!new_entity) {
        // This should not happen if create_entity succeeded, but be defensive.
        return Result<EntityID, std::string>::err("Failed to retrieve newly-created entity " + std::to_string(new_id));
    }

    new_entity->components = src->components;
    new_entity->active = src->active;

    return Result<EntityID, std::string>::ok(new_id);
}

Result<bool, std::string> delete_entity(
    scene::EntityManager& em,
    EntityID id) {

    const scene::Entity* e = em.get_entity(id);
    if (!e) {
        return Result<bool, std::string>::err("Entity " + std::to_string(id) + " not found");
    }

    em.destroy_entity(id);
    return Result<bool, std::string>::ok(true);
}

} // namespace odyssey::editor
