#include "editor/entity_clipboard.h"

#include <spdlog/spdlog.h>

namespace odyssey::editor {

// Global session clipboard
static EntityClipboard g_entity_clipboard;

EntityClipboard& entity_clipboard() {
    return g_entity_clipboard;
}

EntityID clipboard_copy_entity(const scene::Entity& entity) {
    // Generate a new ID for the cloned entity (simple incrementing ID)
    static EntityID next_clipboard_id = 10000;  // Start from a high ID to avoid conflicts
    EntityID new_id = next_clipboard_id++;

    scene::Entity cloned = entity;
    cloned.id = new_id;
    cloned.name += " (Copy)";  // Append (Copy) to the name

    g_entity_clipboard.entities[new_id] = std::move(cloned);
    g_entity_clipboard.is_cut = false;

    spdlog::debug("[entity_clipboard] copied entity (new id={})", new_id);
    return new_id;
}

size_t clipboard_copy_entities(const std::unordered_map<EntityID, scene::Entity>& entities) {
    g_entity_clipboard.entities.clear();

    for (const auto& [orig_id, entity] : entities) {
        (void)orig_id;  // Use entity.id instead
        clipboard_copy_entity(entity);
    }

    g_entity_clipboard.is_cut = false;
    spdlog::debug("[entity_clipboard] copied {} entities", entities.size());
    return entities.size();
}

void clipboard_clear() {
    g_entity_clipboard.entities.clear();
    g_entity_clipboard.is_cut = false;
    spdlog::debug("[entity_clipboard] cleared");
}

} // namespace odyssey::editor
