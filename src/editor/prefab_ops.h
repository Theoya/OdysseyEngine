#pragma once

// ---------------------------------------------------------------------------
// prefab_ops.h
// Prefab operations for the editor (Batch H).
//
// Provides:
//   - create_prefab_from_entity: write entity to .prefab.xml + replace with instance.
//   - open_prefab_in_isolation: stub for Batch I (prefab isolation mode).
//   - apply_prefab_overrides: stub for Batch I (persist instance overrides to source).
//   - revert_prefab_overrides: stub for Batch I (discard instance overrides).
//   - unpack_prefab: replace prefab instance with its constituent entities.
// ---------------------------------------------------------------------------

#include "core/result.h"
#include "core/types.h"
#include "scene/entity_manager.h"

#include <filesystem>

namespace odyssey::editor {

// Create a prefab from an entity: writes a new .prefab.xml file capturing
// the entity's components; replaces the entity with a prefab instance
// (EntityComponents::prefab_source points at the new file).
//
// Returns the path to the newly-written .prefab.xml file on success.
// Errors if the entity doesn't exist or file write fails.
Result<std::filesystem::path, std::string> create_prefab_from_entity(
    const scene::EntityManager& em, EntityID id,
    const std::filesystem::path& prefabs_dir);

// STUB for Batch H: full implementation of prefab-isolation mode lands in Batch I.
// Opens a prefab file in a temporary isolation session (separate EntityManager).
Result<bool, std::string> open_prefab_in_isolation(
    const std::filesystem::path& prefab_path);

// STUB for Batch H: apply overrides from this instance back to its prefab source.
// (Full impl deferred to Batch I.)
Result<bool, std::string> apply_prefab_overrides(
    const scene::EntityManager& em, EntityID id);

// STUB for Batch H: revert this instance to match its prefab source (discard overrides).
// (Full impl deferred to Batch I.)
Result<bool, std::string> revert_prefab_overrides(
    scene::EntityManager& em, EntityID id);

// Replace the prefab instance with its constituent entities (unpack).
// Deletes the instance entity and re-instantiates all children/components.
// If the prefab has no children, returns ok(true) after removing the instance.
Result<bool, std::string> unpack_prefab(scene::EntityManager& em, EntityID id);

} // namespace odyssey::editor
