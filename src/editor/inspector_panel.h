#pragma once

// ---------------------------------------------------------------------------
// inspector_panel.h
// Read-only inspector for the currently-selected entity. Renders every
// EntityComponents field as a labeled row. Editing lands in Phase 2.
// ---------------------------------------------------------------------------

#include "editor/panel.h"

#include <string>

namespace odyssey::editor {

class InspectorPanel : public Panel {
public:
    InspectorPanel();

    const std::string& name() const override { return name_; }
    void draw(EditorState& state) override;

private:
    std::string name_ = "Inspector";
};

} // namespace odyssey::editor
