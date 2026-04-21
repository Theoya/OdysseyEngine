#pragma once

// ---------------------------------------------------------------------------
// inspector_panel.h
// Phase 4 inspector. Writable in Edit mode, read-only in Play/Simulate.
//
// Sub-editors:
//   - Transform  (position, euler rotation, scale)   — drag-float widgets
//   - Stats      (health, max_health, ammo, speed, voice_range) — drag-float
//   - Tags       (repeated free-text strings)        — add/remove rows
//   - Mesh/Material/Behavior/Script                  — read-only paths
//
// When EditorState::selected_asset is non-empty, the panel instead shows
// a preview pane for that file (pretty-print XML / .nadir colored keywords /
// plain text).
//
// Every mutation:
//   1. Writes the new value into entity->components.
//   2. Mirrors the write into the matching SceneData::EntityDesc so the
//      reconstruction path picks it up on save.
//   3. Sets SceneData::mutated = true.
//
// Save path:
//   - Ctrl+S or the "Save Scene" menu button flips
//     EditorState::save_requested; Editor consumes it, calls
//     scene::serialize_scene(scene_data, scene_path), and logs the result.
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

    // The cached preview buffer for the currently-selected asset, so we
    // don't re-read the file every frame. Invalidated by path change.
    std::string preview_path_;
    std::string preview_text_;
    bool        preview_is_nadir_ = false;
};

} // namespace odyssey::editor
