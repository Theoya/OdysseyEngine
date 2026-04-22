#include "editor/play_snapshot.h"

#include "core/types.h"
#include "scene/entity_manager.h"
#include "scene/scene_loader.h"

#include <spdlog/spdlog.h>

namespace odyssey::editor {

Result<PlaySnapshot, std::string> capture_snapshot(
    const scene::SceneData& sd,
    const scene::EntityManager& em) {

    PlaySnapshot snap;
    snap.scene_data_snapshot = sd;  // Deep copy via std::vector copy constructor

    // Clone all entities in the entity manager
    // EntityManager::get_all_entities() returns an unordered_map<EntityID, Entity>
    const auto& live_entities = em.get_all_entities();
    for (const auto& [id, entity] : live_entities) {
        snap.entities_snapshot[id] = entity;  // Deep copy each Entity
    }

    spdlog::debug("[editor] snapshot captured: {} entities", snap.entities_snapshot.size());
    return Result<PlaySnapshot, std::string>::ok(snap);
}

Result<bool, std::string> restore_snapshot(
    const PlaySnapshot& snap,
    scene::SceneData& sd_out,
    scene::EntityManager& em_out) {

    // Restore scene data
    sd_out = snap.scene_data_snapshot;

    // Restore entity manager: clear and rebuild from snapshot
    em_out.restore_entities(snap.entities_snapshot);

    spdlog::debug("[editor] snapshot restored: {} entities", em_out.entity_count());
    return Result<bool, std::string>::ok(true);
}

} // namespace odyssey::editor
