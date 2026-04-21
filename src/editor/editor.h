#pragma once

// ---------------------------------------------------------------------------
// editor.h
// OdysseyEngine Editor — Phase 1.
//
// Responsibilities (Phase 1):
//   - Create a GLFW window + Vulkan context + ImGui overlay.
//   - Load a showcase-style .scene.xml into an EntityManager.
//   - Drive three read-only panels: SceneTree, Inspector, Viewport, Log.
//   - Provide Mode (Play/Edit/Simulate) as editor state — logic hooks are
//     stubbed for Phase 2.
//
// Phase 2+ (NOT this task):
//   - Writable inspector fields (round-trip serialize).
//   - Live viewport image from PostProcessor offscreen view via
//     ImGui_ImplVulkan_AddTexture.
//   - Engine in-process hosting with mode pause semantics.
// ---------------------------------------------------------------------------

#include "core/result.h"
#include "editor/mode_enum.h"
#include "scene/entity_manager.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace odyssey::editor {

class Panel;
class SceneViewportRenderer;

// Editor state — the single pure-data struct passed to every Panel::draw.
// Anything that's worth saving to an editor-layout file lives here.
struct EditorState {
    // Selection
    EntityID selected_entity = INVALID_ENTITY;

    // Execution mode
    Mode mode = Mode::Edit;

    // Loaded scene (owned by Editor; panels hold a reference)
    scene::EntityManager* entities = nullptr;
    std::filesystem::path scene_path;

    // Coarse dirty flag (set when mode changes or selection changes —
    // used by future Phase 2 persistence hooks).
    bool dirty = false;

    // Status-bar text (last log line, load result, etc.)
    std::string status_line;

    // --- Phase 2 viewport integration ---
    // Pointer to the live scene-viewport renderer. ViewportPanel uses this
    // to bind the offscreen image into ImGui and to trigger resizes. Null
    // in tests that construct EditorState without a full editor.
    SceneViewportRenderer* viewport_renderer = nullptr;

    // ImGui-side descriptor set handle returned by ImGui_ImplVulkan_AddTexture.
    // Managed by Editor (lifetime spans the editor session, recreated on
    // offscreen-image resize). Cast to ImTextureID at draw time.
    void* viewport_texture_id = nullptr;

    // ViewportPanel writes the pixel size it would like the offscreen target
    // to be. Editor reads and clears this each frame, applying the resize
    // outside the ImGui frame so no command buffer is in flight.
    // Zero (default) means "no request this frame".
    uint32_t viewport_requested_width  = 0;
    uint32_t viewport_requested_height = 0;

    // --- Phase 4 additions ---
    // Root of the asset tree the Asset Browser walks. Defaults to
    // demo/showcase/ in the Editor ctor; can be swapped at runtime later.
    std::filesystem::path project_root;

    // Currently-selected asset (Asset Browser click → Inspector preview).
    // Empty when nothing is selected; when non-empty takes precedence over
    // the selected-entity inspector body via the Inspector's mode toggle.
    std::filesystem::path selected_asset;

    // Set by the Asset Browser when the user double-clicks a scene file.
    // Editor consumes the request between frames — it cannot reload mid-
    // draw because panels hold bare pointers into EntityManager. Reset to
    // empty after the Editor handles the swap.
    std::filesystem::path scene_swap_request;

    // Set by the "Save Scene" menu button / Ctrl+S shortcut / Inspector.
    // Editor consumes it once per frame, invokes scene_serializer, and
    // emits a log entry. Reset to false after handling.
    bool save_requested = false;

    // The SceneData the editor is currently editing (same underlying bytes
    // as `entities` was populated from; kept here so serialize_scene has
    // the preserved_source + unknown buckets it needs). The Inspector's
    // edits write both into `entities->get_entity(...)->components` AND
    // into the matching `scene_data.entities[i].*` so the reconstruction
    // path sees the mutation.
    void* scene_data = nullptr;  // SceneData*, void* to avoid a header cycle
};

// Pure helper: given an entity pointer, produce a stable display label.
// Exposed here so unit tests can exercise the label format.
std::string entity_display_label(const scene::Entity& entity);

// Pure helper: returns true if a given archetype name should be grouped
// under the "Static Geometry" header in the scene tree.
bool is_static_archetype(const std::string& archetype);

class Editor {
public:
    Editor();
    ~Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    // Initialize window, Vulkan, ImGui. Loads the given scene file if
    // present (scene load errors are logged but not fatal — per the
    // Phase 1 spec, the editor should still open).
    Result<bool> initialize(const std::filesystem::path& scene_path);

    // Run the main loop until the user closes the window.
    void run();

    // Tear down in reverse initialization order.
    void shutdown();

    // Impl is declared public so the .cpp's free-function Vulkan helpers
    // can accept `Editor::Impl&` directly. Nothing else references it.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;

    std::vector<std::unique_ptr<Panel>> panels_;
    EditorState state_;

    void build_panels();
    void draw_frame(float delta_time);
    void draw_menu_bar();
    void draw_mode_toolbar();
    void draw_status_bar();
};

} // namespace odyssey::editor
