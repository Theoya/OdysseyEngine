#pragma once

#include "scene/entity_manager.h"

#include <unordered_map>
#include <optional>

namespace odyssey::editor {

// Session clipboard for entity copy/paste. Holds a set of cloned entities
// ready to be pasted into the scene.
struct EntityClipboard {
    // Map of cloned entities (may be empty if nothing copied)
    std::unordered_map<EntityID, scene::Entity> entities;

    // True if the clipboard was populated by a cut (not copy).
    // Paste after cut will "move" the entities rather than duplicate.
    bool is_cut = false;
};

// Returns the global session clipboard. Mutable singleton; same instance
// persists across all inspector edits in a session.
EntityClipboard& entity_clipboard();

// Pure helpers for entity copy/paste.

// Copy an entity into the clipboard. Clones the entity and stores it.
// Returns the cloned entity's ID (may differ from original).
EntityID clipboard_copy_entity(const scene::Entity& entity);

// Copy multiple entities into the clipboard (for multi-select paste).
// Returns the count of cloned entities.
size_t clipboard_copy_entities(const std::unordered_map<EntityID, scene::Entity>& entities);

// Clear the clipboard (used after pasting in Cut mode or manually).
void clipboard_clear();

} // namespace odyssey::editor
