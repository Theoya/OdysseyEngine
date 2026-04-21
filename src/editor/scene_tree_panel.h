#pragma once

// ---------------------------------------------------------------------------
// scene_tree_panel.h
// Tree-view of all entities in the loaded scene. Clicking an entity updates
// EditorState::selected_entity.
// ---------------------------------------------------------------------------

#include "editor/panel.h"

#include <string>

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
};

} // namespace odyssey::editor
