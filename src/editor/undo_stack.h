#pragma once

#include "core/types.h"
#include "scene/scene_loader.h"
#include "scene/entity_manager.h"

#include <deque>
#include <unordered_map>
#include <string>

namespace odyssey::editor {

// A single undo/redo entry: full snapshot of scene state + description.
// We store the entire SceneData and EntityManager state at the moment
// of capture, so any restoration is a complete rollback (no partial edits).
struct UndoEntry {
    std::string description;  // "Move Entity", "Delete Entity", "Add Component", etc.
    scene::SceneData state;   // Full snapshot of scene data
    std::unordered_map<EntityID, scene::Entity> entities;  // Full snapshot of entity map
};

// Undo/redo stack with a maximum depth of 64 entries.
// Follows the standard undo/redo pattern: pushing a new entry discards any
// future redo history.
class UndoStack {
public:
    static constexpr size_t kMaxDepth = 64;

    // Push a new undo entry. Discards all redo history.
    void push(UndoEntry entry);

    // True if we can undo (past_ is not empty).
    bool can_undo() const;

    // True if we can redo (future_ is not empty).
    bool can_redo() const;

    // Peek at the next undo entry without removing it. Returns nullptr if empty.
    const UndoEntry* peek_undo() const;

    // Peek at the next redo entry without removing it. Returns nullptr if empty.
    const UndoEntry* peek_redo() const;

    // Pop and return the next undo entry. Moves it to future_.
    // Precondition: can_undo() must be true. Behavior undefined if violated.
    UndoEntry pop_undo();

    // Pop and return the next redo entry. Moves it to past_.
    // Precondition: can_redo() must be true. Behavior undefined if violated.
    UndoEntry pop_redo();

    // Clear all undo and redo history.
    void clear();

    // Query current undo stack depth.
    size_t undo_count() const { return past_.size(); }

    // Query current redo stack depth.
    size_t redo_count() const { return future_.size(); }

private:
    std::deque<UndoEntry> past_;    // Undo stack (oldest at front)
    std::deque<UndoEntry> future_;  // Redo stack (newest at front)
};

} // namespace odyssey::editor
