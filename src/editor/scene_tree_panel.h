#pragma once

// ---------------------------------------------------------------------------
// scene_tree_panel.h
// Tree-view of all entities in the loaded scene. Clicking an entity updates
// EditorState::selected_entity.
// ---------------------------------------------------------------------------

#include "editor/panel.h"
#include "core/types.h"

#include <string>
#include <unordered_map>

namespace odyssey::editor {

class SceneTreePanel : public Panel {
public:
    SceneTreePanel();

    const std::string& name() const override { return name_; }
    void draw(EditorState& state) override;

private:
    std::string name_ = "Scene Tree";

    // Simple search-filter string (applied live as the user types).
    std::string filter_;

    // Batch B: track expand/collapse state per archetype (for expand-all/collapse-all)
    std::unordered_map<std::string, bool> archetype_expanded_;

    // Batch B: rename-in-progress tracking
    EntityID rename_in_progress_ = INVALID_ENTITY;
    std::string rename_buffer_;
};

} // namespace odyssey::editor
