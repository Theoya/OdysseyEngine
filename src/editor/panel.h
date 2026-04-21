#pragma once

// ---------------------------------------------------------------------------
// panel.h
// Base Panel interface. Every editor panel is a small owned object that
// knows how to draw itself inside an ImGui window. Panels are owned by the
// Editor class and are polled each frame.
//
// Design rule: Panel::draw() is THE I/O boundary. Anything pure belongs in
// free functions in the panel's .cpp (layout math, filter predicates,
// label generation). Anything impure (ImGui calls, Vulkan texture bind)
// lives inside draw().
// ---------------------------------------------------------------------------

#include <string>

namespace odyssey::editor {

class EditorState;

class Panel {
public:
    virtual ~Panel() = default;

    // Stable, unique panel name — used as the ImGui window label and as
    // the persisted layout key.
    virtual const std::string& name() const = 0;

    // Draw the panel. Assumed to be called inside a valid ImGui frame.
    // The panel is responsible for its own ImGui::Begin/End pair.
    virtual void draw(EditorState& state) = 0;

    // Optional per-frame update (for things that need a tick but don't
    // belong in the draw boundary — e.g. log buffer rotation).
    virtual void tick(float /*delta_time*/) {}

    // Visibility toggle. The editor menu bar flips this.
    bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

protected:
    bool visible_ = true;
};

} // namespace odyssey::editor
