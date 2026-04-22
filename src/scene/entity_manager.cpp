#include "scene/entity_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <unordered_set>

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

void EntityManager::restore_entities(
    const std::unordered_map<EntityID, Entity>& snapshot_entities) {
    clear();
    entities_ = snapshot_entities;

    // Rebuild archetype groups from the restored entities
    for (const auto& [id, entity] : entities_) {
        (void)id;  // Entity ID is already in the entity struct
        auto& group = get_or_create_archetype(entity.archetype);
        group.entity_ids.push_back(entity.id);
        // Preserve the behavior_shader from the entity if set
        if (!entity.components.behavior_shader.empty() && group.behavior_shader.empty()) {
            group.behavior_shader = entity.components.behavior_shader;
        }
    }

    // Update next_id_ to be greater than any existing entity ID
    for (const auto& [id, entity] : entities_) {
        if (id >= next_id_) {
            next_id_ = id + 1;
        }
    }

    spdlog::debug("[entity_manager] restored {} entities", entities_.size());
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

// Phase 9: Compose world transforms via topological sort + recursive composition.
// world = parent.world × local (quaternion-based, SoA-friendly).
// Derivation: For a parent entity P with world transform Pw and local transform PL,
// and child entity C with local transform CL, the child's world transform is:
//   Cw = Pw × CL, where × means: translate by Pw.pos, rotate by Pw.rot, scale by Pw.scale,
//   then apply CL in that frame. Achieved via matrix composition then extraction.
Result<bool, HierarchyError> EntityManager::compose_world_transforms() {
    // Validation: check for self-parents, unknown parents.
    for (const auto& [id, entity] : entities_) {
        if (entity.parent_id == INVALID_ENTITY) continue;

        if (entity.parent_id == id) {
            spdlog::error("Entity {} is its own parent", id);
            return Result<bool, HierarchyError>::err(HierarchyError::SelfParent);
        }

        if (entities_.find(entity.parent_id) == entities_.end()) {
            spdlog::error("Entity {} has unknown parent {}", id, entity.parent_id);
            return Result<bool, HierarchyError>::err(HierarchyError::UnknownParent);
        }
    }

    // Helper: compute depth of an entity (chain length from root)
    // Returns error on cycle or depth > 64.
    auto compute_depth = [this](EntityID id) -> Result<int, HierarchyError> {
        std::unordered_set<EntityID> visited;
        EntityID current = id;
        int depth = 0;

        while (current != INVALID_ENTITY) {
            if (visited.find(current) != visited.end()) {
                return Result<int, HierarchyError>::err(HierarchyError::Cycle);
            }
            if (depth > 64) {
                return Result<int, HierarchyError>::err(HierarchyError::DepthExceeded);
            }

            visited.insert(current);
            Entity* e = get_entity(current);
            if (!e) {
                return Result<int, HierarchyError>::err(HierarchyError::UnknownParent);
            }

            current = e->parent_id;
            depth++;
        }

        return Result<int, HierarchyError>::ok(depth - 1);
    };

    // Check all entities for depth violations
    for (const auto& [id, entity] : entities_) {
        (void)entity;
        auto depth_result = compute_depth(id);
        if (depth_result.is_err()) {
            return Result<bool, HierarchyError>::err(depth_result.error());
        }
    }

    // Topological sort via DFS + compose transforms
    std::unordered_set<EntityID> visited;

    std::function<Result<bool, HierarchyError>(EntityID)> dfs_compose =
        [&](EntityID id) -> Result<bool, HierarchyError> {
        if (visited.find(id) != visited.end()) {
            return Result<bool, HierarchyError>::ok(true);
        }

        Entity* entity = get_entity(id);
        if (!entity) {
            return Result<bool, HierarchyError>::err(HierarchyError::UnknownParent);
        }

        // If this entity has a parent, compose parent first, then apply parent's world.
        if (entity->parent_id != INVALID_ENTITY) {
            auto r = dfs_compose(entity->parent_id);
            if (r.is_err()) {
                return r;
            }

            Entity* parent = get_entity(entity->parent_id);
            if (!parent) {
                return Result<bool, HierarchyError>::err(HierarchyError::UnknownParent);
            }

            // Compose: world = parent.world × local
            // Build matrices from transforms and multiply.
            // Parent world matrix (already computed in recursion)
            glm::mat4 parent_world = glm::mat4_cast(parent->world_transform.rotation);
            parent_world = glm::translate(glm::mat4(1.0f), parent->world_transform.position) *
                           parent_world *
                           glm::scale(glm::mat4(1.0f), parent->world_transform.scale);

            // Local transform of this entity
            glm::mat4 local = glm::mat4_cast(entity->components.transform.rotation);
            local = glm::translate(glm::mat4(1.0f), entity->components.transform.position) *
                    local *
                    glm::scale(glm::mat4(1.0f), entity->components.transform.scale);

            // Composite
            glm::mat4 world = parent_world * local;

            // Extract: position from column 3, rotation from upper-left 3x3,
            // scale from column magnitudes.
            entity->world_transform.position = glm::vec3(world[3]);
            entity->world_transform.rotation = glm::quat_cast(glm::mat3(world));
            entity->world_transform.scale = glm::vec3(
                glm::length(glm::vec3(world[0])),
                glm::length(glm::vec3(world[1])),
                glm::length(glm::vec3(world[2]))
            );
        } else {
            // Top-level: world = local
            entity->world_transform = entity->components.transform;
        }

        visited.insert(id);
        return Result<bool, HierarchyError>::ok(true);
    };

    // Traverse all entities
    for (const auto& [id, entity] : entities_) {
        (void)entity;
        if (visited.find(id) == visited.end()) {
            auto r = dfs_compose(id);
            if (r.is_err()) {
                return r;
            }
        }
    }

    return Result<bool, HierarchyError>::ok(true);
}

} // namespace odyssey::scene
