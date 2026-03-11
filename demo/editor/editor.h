#pragma once

#include "app/engine.h"
#include "scene/entity_manager.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>

// Forward-declare ImGui types
struct ImGuiContext;

namespace odyssey {
namespace editor {

// Forward declarations
class HierarchyPanel;
class InspectorPanel;
class ViewportPanel;
class ConsolePanel;
class AIAssistantPanel;
class Toolbar;
class MenuBar;

/// Entity representation for the editor
struct EditorEntity {
    EntityId id;
    std::string name;
    std::vector<EditorEntity> children;
    bool expanded = true;
};

/// Selection state
struct SelectionState {
    std::optional<EntityId> selected_entity;
    std::vector<EntityId> multi_selected;
};

/// Editor mode
enum class EditorMode {
    Edit,   // Editor mode - can select, transform
    Play,   // Game running
    Pause,  // Game paused
};

/// Gizmo mode for transform tools
enum class GizmoMode {
    Translate,
    Rotate,
    Scale,
};

/// Main Editor class - orchestrates all panels
class Editor {
public:
    Editor();
    ~Editor();

    // Non-copyable
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    /// Initialize editor with engine
    bool initialize(Engine* engine);

    /// Main editor frame - called every frame
    void update(float delta_time);

    /// Render all panels
    void render();

    /// Handle input
    void handle_input();

    /// Shutdown
    void shutdown();

    // --- Editor State ---

    EditorMode get_mode() const { return mode_; }
    void set_mode(EditorMode mode) { mode_ = mode; }

    GizmoMode get_gizmo_mode() const { return gizmo_mode_; }
    void set_gizmo_mode(GizmoMode mode) { gizmo_mode_ = mode; }

    const SelectionState& get_selection() const { return selection_; }
    void select_entity(EntityId id);
    void clear_selection();

    // --- Entity Operations ---

    EntityId create_entity(const std::string& name = "New Entity");
    void delete_entity(EntityId id);
    void duplicate_entity(EntityId id);

    // --- Console Logging ---

    void log_info(const std::string& msg);
    void log_warning(const std::string& msg);
    void log_error(const std::string& msg);

private:
    // Panel rendering
    void render_menu_bar();
    void render_dock_space();
    void render_toolbar();

    // Initialize panels
    void init_panels();
    void shutdown_panels();

    // Engine reference (not owned)
    Engine* engine_ = nullptr;

    // Editor state
    EditorMode mode_ = EditorMode::Edit;
    GizmoMode gizmo_mode_ = GizmoMode::Translate;
    SelectionState selection_;

    // Panels (owned)
    std::unique_ptr<MenuBar> menu_bar_;
    std::unique_ptr<Toolbar> toolbar_;
    std::unique_ptr<HierarchyPanel> hierarchy_panel_;
    std::unique_ptr<InspectorPanel> inspector_panel_;
    std::unique_ptr<ViewportPanel> viewport_panel_;
    std::unique_ptr<ConsolePanel> console_panel_;
    std::unique_ptr<AIAssistantPanel> ai_assistant_panel_;

    // ImGui context
    ImGuiContext* imgui_context_ = nullptr;

    // Entity manager
    std::unique_ptr<EntityManager> entity_manager_;
};

// Helper to create the editor
std::unique_ptr<Editor> create_editor();

} // namespace editor
} // namespace odyssey
