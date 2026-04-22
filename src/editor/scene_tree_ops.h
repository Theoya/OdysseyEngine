#pragma once

#include "core/result.h"
#include "core/types.h"
#include "scene/entity_manager.h"

#include <string>

namespace odyssey::editor {

// Pure helper: test if an entity name or archetype matches a filter string
// (case-insensitive substring match).
bool matches_filter(const scene::Entity& entity, const std::string& filter);

// Duplicate an entity: clone its EntityComponents, create a new entity with
// the same archetype and a name suffix of " (Copy)". Returns the new
// entity's ID, or Err if entity_id doesn't exist.
Result<EntityID, std::string> duplicate_entity(
    scene::EntityManager& em,
    EntityID src_id);

// Delete an entity from the EntityManager. Returns Ok if successful,
// Err if entity_id doesn't exist.
Result<bool, std::string> delete_entity(
    scene::EntityManager& em,
    EntityID id);

} // namespace odyssey::editor
