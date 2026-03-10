#include "scene/entity_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace odyssey::scene {

EntityID EntityManager::create_entity(const std::string& name, const std::string& archetype) {
    EntityID id = next_id_++;

    Entity entity;
    entity.id = id;
    entity.name = name;
    entity.archetype = archetype;
    entity.active = true;

    entities_.emplace(id, std::move(entity));

    // Add to archetype group
    auto& group = get_or_create_archetype(archetype);
    group.entity_ids.push_back(id);

    spdlog::debug("Created entity '{}' (id={}) with archetype '{}'", name, id, archetype);
    return id;
}

std::vector<EntityID> EntityManager::create_entities(const std::string& archetype, uint32_t count,
                                                      const std::string& name_prefix) {
    std::vector<EntityID> ids;
    ids.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        std::string name = name_prefix + "_" + std::to_string(i);
        ids.push_back(create_entity(name, archetype));
    }

    spdlog::info("Batch created {} entities of archetype '{}'", count, archetype);
    return ids;
}

void EntityManager::destroy_entity(EntityID id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        spdlog::warn("Attempted to destroy nonexistent entity id={}", id);
        return;
    }

    const std::string& archetype = it->second.archetype;

    // Remove from archetype group
    auto idx_it = archetype_index_.find(archetype);
    if (idx_it != archetype_index_.end()) {
        auto& group = archetype_groups_[idx_it->second];
        auto& ids = group.entity_ids;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    }

    spdlog::debug("Destroyed entity '{}' (id={})", it->second.name, id);
    entities_.erase(it);
}

Entity* EntityManager::get_entity(EntityID id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) return nullptr;
    return &it->second;
}

const Entity* EntityManager::get_entity(EntityID id) const {
    auto it = entities_.find(id);
    if (it == entities_.end()) return nullptr;
    return &it->second;
}

Entity* EntityManager::find_entity(const std::string& name) {
    for (auto& [id, entity] : entities_) {
        if (entity.name == name) {
            return &entity;
        }
    }
    return nullptr;
}

const ArchetypeGroup* EntityManager::get_archetype_group(const std::string& archetype) const {
    auto it = archetype_index_.find(archetype);
    if (it == archetype_index_.end()) return nullptr;
    return &archetype_groups_[it->second];
}

void EntityManager::clear() {
    entities_.clear();
    archetype_groups_.clear();
    archetype_index_.clear();
    next_id_ = 0;
    next_archetype_id_ = 0;
    spdlog::debug("EntityManager cleared");
}

ArchetypeGroup& EntityManager::get_or_create_archetype(const std::string& archetype) {
    auto it = archetype_index_.find(archetype);
    if (it != archetype_index_.end()) {
        return archetype_groups_[it->second];
    }

    // Create new archetype group
    ArchetypeGroup group;
    group.archetype_name = archetype;
    group.id = next_archetype_id_++;

    size_t index = archetype_groups_.size();
    archetype_groups_.push_back(std::move(group));
    archetype_index_[archetype] = index;

    spdlog::info("Created archetype group '{}' (id={})", archetype, archetype_groups_[index].id);
    return archetype_groups_[index];
}

} // namespace odyssey::scene
