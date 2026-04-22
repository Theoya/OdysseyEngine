#pragma once

#include "core/result.h"
#include "scene/entity_manager.h"
#include "scene/scene_loader.h"

#include <unordered_map>
#include <string>

namespace odyssey::editor {

// Deep clone of SceneData + EntityManager at the moment Play/Simulate starts.
// Used by play-in-editor to snapshot the initial state and restore it on Stop.
struct PlaySnapshot {
    // Full snapshot of the scene data (lighting, metadata, etc.)
    scene::SceneData scene_data_snapshot;

    // Clone of all entities' components. We don't clone the EntityManager
    // instance itself, just its entity map contents — we restore fields back
    // into the live instance.
    std::unordered_map<EntityID, scene::Entity> entities_snapshot;
};

// Pure: captures a deep clone of the scene and entity manager at the current moment.
// Returns Err only if entity lookup fails (should not happen).
Result<PlaySnapshot, std::string> capture_snapshot(
    const scene::SceneData& sd,
    const scene::EntityManager& em);

// Pure: restores a snapshot into output parameters.
// Overwrites sd_out and reshapes em_out to match the snapshot.
// Returns Err if the restoration fails (e.g., invalid entity data).
// The caller is responsible for assigning sd_out and em_out back into the live
// scene (scene::SceneData& state_.scene_data, scene::EntityManager state_.entities).
Result<bool, std::string> restore_snapshot(
    const PlaySnapshot& snap,
    scene::SceneData& sd_out,
    scene::EntityManager& em_out);

} // namespace odyssey::editor
