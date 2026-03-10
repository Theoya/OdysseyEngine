#pragma once
#include "core/types.h"
#include "core/result.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace odyssey::scene {

// Component types an entity can have
struct EntityComponents {
    Transform transform;
    EntityStats stats;
    std::string behavior_shader;  // .nadir file path
    std::string mesh_path;
    std::string material_path;
    std::string script_class;
    std::string script_config;
    std::string prefab_source;    // which prefab it came from
};

// A single entity instance
struct Entity {
    EntityID id = INVALID_ENTITY;
    std::string name;
    std::string archetype;
    EntityComponents components;
    bool active = true;
};

// All entities of one archetype (for batch GPU dispatch)
struct ArchetypeGroup {
    std::string archetype_name;
    ArchetypeID id = INVALID_ARCHETYPE;
    std::vector<EntityID> entity_ids;
    std::string behavior_shader;
};

class EntityManager {
public:
    // Create a new entity, returns its ID
    EntityID create_entity(const std::string& name, const std::string& archetype);

    // Create N entities of same archetype (batch spawn)
    std::vector<EntityID> create_entities(const std::string& archetype, uint32_t count,
                                          const std::string& name_prefix = "entity");

    // Destroy an entity
    void destroy_entity(EntityID id);

    // Get entity by ID (returns nullptr if not found)
    Entity* get_entity(EntityID id);
    const Entity* get_entity(EntityID id) const;

    // Get entity by name
    Entity* find_entity(const std::string& name);

    // Get all entities of an archetype
    const ArchetypeGroup* get_archetype_group(const std::string& archetype) const;

    // Get all archetype groups
    const std::vector<ArchetypeGroup>& get_archetype_groups() const { return archetype_groups_; }

    // Total entity count
    uint32_t entity_count() const { return static_cast<uint32_t>(entities_.size()); }

    // Get all entities (for iteration)
    const std::unordered_map<EntityID, Entity>& get_all_entities() const { return entities_; }

    // Clear everything
    void clear();

private:
    std::unordered_map<EntityID, Entity> entities_;
    std::vector<ArchetypeGroup> archetype_groups_;
    std::unordered_map<std::string, size_t> archetype_index_; // name -> index in groups
    EntityID next_id_ = 0;
    ArchetypeID next_archetype_id_ = 0;

    ArchetypeGroup& get_or_create_archetype(const std::string& archetype);
};

} // namespace odyssey::scene
