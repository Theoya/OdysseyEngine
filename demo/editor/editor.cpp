#include "editor.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

namespace odyssey {
namespace editor {

// ============================================================================
// Menu Bar
// ============================================================================

class MenuBar {
public:
    void render() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                    spdlog::info("Menu: New Scene");
                }
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                    spdlog::info("Menu: Open Scene");
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    spdlog::info("Menu: Save");
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    spdlog::info("Menu: Save As");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    // Handle exit
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                    spdlog::info("Menu: Undo");
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                    spdlog::info("Menu: Redo");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
                if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
                if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
                if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del")) {}
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Show Grid", nullptr, true)) {
                    spdlog::info("Menu: Toggle Grid");
                }
                if (ImGui::MenuItem("Show Axes", nullptr, true)) {
                    spdlog::info("Menu: Toggle Axes");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Layout")) {
                    spdlog::info("Menu: Reset Layout");
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Game")) {
                if (ImGui::MenuItem("Play", "F5")) {
                    spdlog::info("Menu: Play");
                }
                if (ImGui::MenuItem("Pause", "F6")) {
                    spdlog::info("Menu: Pause");
                }
                if (ImGui::MenuItem("Stop", "F7")) {
                    spdlog::info("Menu: Stop");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Step Forward", "F8")) {
                    spdlog::info("Menu: Step");
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("AI Assistant")) {
                    spdlog::info("Menu: AI Assistant");
                }
                if (ImGui::MenuItem("Behavior Editor")) {
                    spdlog::info("Menu: Behavior Editor");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("CLI Console")) {
                    spdlog::info("Menu: CLI Console");
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Documentation")) {}
                if (ImGui::MenuItem("About OdysseyEngine")) {}
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }
    }
};

// ============================================================================
// Toolbar
// ============================================================================

class Toolbar {
public:
    Toolbar(Editor* editor) : editor_(editor) {}
    
    void render() {
        ImGui::BeginToolbar();
        
        // Play controls
        ImGui::ToolbarButton(editor_->get_mode() == EditorMode::Play ? ICON_PLAY_FILLED : ICON_PLAY, "Play (F5)");
        ImGui::ToolbarButton(ICON_PAUSE, "Pause (F6)");
        ImGui::ToolbarButton(ICON_STOP, "Stop (F7)");
        ImGui::Separator();
        
        // Gizmo modes
        bool is_translate = editor_->get_gizmo_mode() == GizmoMode::Translate;
        bool is_rotate = editor_->get_gizmo_mode() == GizmoMode::Rotate;
        bool is_scale = editor_->get_gizmo_mode() == GizmoMode::Scale;
        
        if (ImGui::ToolbarToggle(ICON_MOVE, is_translate, "Translate (G)")) {
            editor_->set_gizmo_mode(GizmoMode::Translate);
        }
        if (ImGui::ToolbarToggle(ICON_ROTATE, is_rotate, "Rotate (R)")) {
            editor_->set_gizmo_mode(GizmoMode::Rotate);
        }
        if (ImGui::ToolbarToggle(ICON_SCALE, is_scale, "Scale (S)")) {
            editor_->set_gizmo_mode(GizmoMode::Scale);
        }
        
        ImGui::Separator();
        
        // Pivot modes
        ImGui::ToolbarButton(ICON_PIVOT, "Pivot");
        ImGui::ToolbarButton(ICON_CENTER, "Center");
        
        ImGui::EndToolbar();
    }
    
private:
    Editor* editor_;
    
    // Icon placeholders (would use a proper icon font in production)
    static constexpr const char* ICON_PLAY = "▶";
    static constexpr const char* ICON_PLAY_FILLED = "▶";
    static constexpr const char* ICON_PAUSE = "⏸";
    static constexpr const char* ICON_STOP = "⏹";
    static constexpr const char* ICON_MOVE = "↔";
    static constexpr const char* ICON_ROTATE = "↻";
    static constexpr const char* ICON_SCALE = "⤢";
    static constexpr const char* ICON_PIVOT = "◎";
    static constexpr const char* ICON_CENTER = "⊙";
};

// ============================================================================
// Hierarchy Panel
// ============================================================================

class HierarchyPanel {
public:
    HierarchyPanel(Editor* editor) : editor_(editor) {}
    
    void render() {
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
        
        // Search bar
        static char search_buf[256] = "";
        ImGui::InputText("##search", search_buf, sizeof(search_buf), ImGuiInputTextFlags_None);
        
        ImGui::Separator();
        
        // Tree
        if (ImGui::BeginChild("tree")) {
            render_entity_tree(EntityId{1}, 0); // Root
            ImGui::EndChild();
        }
        
        // Bottom bar
        ImGui::Separator();
        if (ImGui::Button("+ Create")) {
            editor_->create_entity("New Entity");
        }
        
        ImGui::End();
    }
    
private:
    void render_entity_tree(EntityId parent, int depth) {
        // In a real implementation, this would iterate the entity hierarchy
        // For demo, show some sample entities
        static const char* demo_entities[] = {
            "Main Camera",
            "Directional Light",
            "Player",
            "Ground",
            "Obstacles",
            "UI Canvas"
        };
        
        for (int i = 0; i < 6; i++) {
            bool is_selected = editor_->get_selection().selected_entity.has_value() && 
                              editor_->get_selection().selected_entity->id == i;
            
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
            if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;
            if (i < 2) flags |= ImGuiTreeNodeFlags_DefaultOpen;
            
            // Leaf nodes for demo
            if (i >= 2 && i <= 4) {
                flags |= ImGuiTreeNodeFlags_Leaf;
                bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, demo_entities[i]);
                
                if (ImGui::IsItemClicked()) {
                    editor_->select_entity(EntityId{(uint32_t)i});
                }
                
                if (open) {
                    ImGui::TreePop();
                }
            } else {
                bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, demo_entities[i]);
                
                if (ImGui::IsItemClicked()) {
                    editor_->select_entity(EntityId{(uint32_t)i});
                }
                
                if (open) {
                    ImGui::TreePop();
                }
            }
        }
    }
    
    Editor* editor_;
};

// ============================================================================
// Inspector Panel  
// ============================================================================

class InspectorPanel {
public:
    InspectorPanel(Editor* editor) : editor_(editor) {}
    
    void render() {
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
        
        if (!editor_->get_selection().selected_entity.has_value()) {
            ImGui::TextDisabled("Select an entity to inspect");
            ImGui::End();
            return;
        }
        
        auto selected = editor_->get_selection().selected_entity;
        ImGui::Text("Selected: Entity %u", selected->id);
        
        ImGui::Separator();
        
        // Transform Component (always shown)
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            
            static float position[3] = {0, 0, 0};
            static float rotation[3] = {0, 0, 0};
            static float scale[3] = {1, 1, 1};
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Position");
            ImGui::SameLine(80);
            ImGui::InputFloat3("##pos", position);
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Rotation");
            ImGui::SameLine(80);
            ImGui::InputFloat3("##rot", rotation);
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Scale");
            ImGui::SameLine(80);
            ImGui::InputFloat3("##scale", scale);
            
            ImGui::Unindent();
        }
        
        // Mesh Component
        if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_None)) {
            ImGui::Indent();
            
            static const char* mesh_items[] = {"Cube", "Sphere", "Cylinder", "Capsule", "Plane", "Custom..."};
            static int mesh_idx = 0;
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Mesh");
            ImGui::SameLine(80);
            ImGui::Combo("##mesh", &mesh_idx, mesh_items, IM_ARRAYSIZE(mesh_items));
            
            static const char* mat_items[] = {"Default", "Red", "Green", "Blue", "Metallic", "Emissive"};
            static int mat_idx = 0;
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Material");
            ImGui::SameLine(80);
            ImGui::Combo("##mat", &mat_idx, mat_items, IM_ARRAYSIZE(mat_items));
            
            ImGui::Unindent();
        }
        
        // Light Component
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_None)) {
            ImGui::Indent();
            
            static const char* light_types[] = {"Directional", "Point", "Spot"};
            static int light_idx = 0;
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Type");
            ImGui::SameLine(80);
            ImGui::Combo("##lighttype", &light_idx, light_types, IM_ARRAYSIZE(light_types));
            
            static float color[3] = {1.0f, 0.95f, 0.8f};
            static float intensity = 1.0f;
            static float range = 10.0f;
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Color");
            ImGui::SameLine(80);
            ImGui::ColorEdit3("##color", color);
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Intensity");
            ImGui::SameLine(80);
            ImGui::SliderFloat("##intensity", &intensity, 0.0f, 10.0f);
            
            if (light_idx > 0) { // Point or Spot
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Range");
                ImGui::SameLine(80);
                ImGui::SliderFloat("##range", &range, 0.1f, 100.0f);
            }
            
            ImGui::Unindent();
        }
        
        // Camera Component
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None)) {
            ImGui::Indent();
            
            static float fov = 60.0f;
            static float near = 0.1f;
            static float far = 1000.0f;
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("FOV");
            ImGui::SameLine(80);
            ImGui::SliderFloat("##fov", &fov, 10.0f, 120.0f);
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Near");
            ImGui::SameLine(80);
            ImGui::InputFloat("##near", &near);
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Far");
            ImGui::SameLine(80);
            ImGui::InputFloat("##far", &far);
            
            static bool is_main = true;
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Main");
            ImGui::SameLine(80);
            ImGui::Checkbox("##main", &is_main);
            
            ImGui::Unindent();
        }
        
        // Behavior Component
        if (ImGui::CollapsingHeader("Behavior", ImGuiTreeNodeFlags_None)) {
            ImGui::Indent();
            
            static const char* behavior_items[] = {"None", "PlayerController", "EnemyAI", "Patrol", "LookAt", "Follow"};
            static int behavior_idx = 0;
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Script");
            ImGui::SameLine(80);
            ImGui::Combo("##behavior", &behavior_idx, behavior_items, IM_ARRAYSIZE(behavior_items));
            
            if (behavior_idx > 0) {
                ImGui::Separator();
                ImGui::TextDisabled("Parameters:");
                
                static float speed = 5.0f;
                static float radius = 2.0f;
                
                if (behavior_idx == 1 || behavior_idx == 2) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Speed");
                    ImGui::SameLine(80);
                    ImGui::SliderFloat("##speed", &speed, 0.0f, 20.0f);
                }
                
                if (behavior_idx == 4 || behavior_idx == 5) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Radius");
                    ImGui::SameLine(80);
                    ImGui::SliderFloat("##radius", &radius, 0.1f, 10.0f);
                }
            }
            
            ImGui::Unindent();
        }
        
        // Tags & Layers
        if (ImGui::CollapsingHeader("Tags & Layers", ImGuiTreeNodeFlags_None)) {
            ImGui::Indent();
            
            static const char* tags[] = {"Untagged", "Player", "Enemy", "Projectile", "Environment"};
            static int tag_idx = 0;
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Tag");
            ImGui::SameLine(80);
            ImGui::Combo("##tag", &tag_idx, tags, IM_ARRAYSIZE(tags));
            
            static bool layer_flags[4] = {false, false, false, false};
            static const char* layers[] = {"Default", "Water", "Player", "Enemy"};
            
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Layers");
            ImGui::SameLine(80);
            ImGui::BeginGroup();
            for (int i = 0; i < 4; i++) {
                ImGui::Checkbox(layers[i], &layer_flags[i]);
            }
            ImGui::EndGroup();
            
            ImGui::Unindent();
        }
        
        ImGui::End();
    }
    
private:
    Editor* editor_;
};

// ============================================================================
// Viewport Panel
// ============================================================================

class ViewportPanel {
public:
    ViewportPanel(Editor* editor) : editor_(editor), show_grid_(true), show_axes_(true) {}
    
    void render() {
        ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse);
        
        // Viewport toolbar
        ImGui::SameLine();
        if (ImGui::Checkbox("Grid", &show_grid_)) {}
        ImGui::SameLine();
        if (ImGui::Checkbox("Axes", &show_axes_)) {}
        ImGui::SameLine();
        
        static const char* view_modes[] = {"Perspective", "Top", "Front", "Right"};
        ImGui::Combo("##view", &view_mode_idx_, view_modes, IM_ARRAYSIZE(view_modes));
        
        ImGui::Separator();
        
        // Viewport area
        ImVec2 viewport_size = ImGui::GetContentRegionAvail();
        
        // Draw a placeholder viewport with grid
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        
        // Background gradient (sky simulation)
        ImU32 sky_top = IM_COL32(100, 149, 237, 255);
        ImU32 sky_bottom = IM_COL32(176, 196, 222, 255);
        draw_list->AddRectFilledMultiColor(
            canvas_pos,
            ImVec2(canvas_pos.x + viewport_size.x, canvas_pos.y + viewport_size.y),
            sky_top, sky_top, sky_bottom, sky_bottom
        );
        
        // Grid
        if (show_grid_) {
            ImU32 grid_color = IM_COL32(100, 100, 100, 80);
            float grid_spacing = 30.0f;
            
            for (float x = 0; x < viewport_size.x; x += grid_spacing) {
                draw_list->AddLine(
                    ImVec2(canvas_pos.x + x, canvas_pos.y),
                    ImVec2(canvas_pos.x + x, canvas_pos.y + viewport_size.y),
                    grid_color
                );
            }
            for (float y = 0; y < viewport_size.y; y += grid_spacing) {
                draw_list->AddLine(
                    ImVec2(canvas_pos.x, canvas_pos.y + y),
                    ImVec2(canvas_pos.x + viewport_size.x, canvas_pos.y + y),
                    grid_color
                );
            }
            
            // Ground line
            float ground_y = canvas_pos.y + viewport_size.y * 0.7f;
            draw_list->AddLine(
                ImVec2(canvas_pos.x, ground_y),
                ImVec2(canvas_pos.x + viewport_size.x, ground_y),
                IM_COL32(150, 150, 150, 255),
                2.0f
            );
        }
        
        // Axes (origin indicator)
        if (show_axes_) {
            ImVec2 origin = ImVec2(canvas_pos.x + 80, canvas_pos.y + viewport_size.y * 0.7f - 20);
            float axis_len = 40.0f;
            
            // X axis (red)
            draw_list->AddLine(origin, ImVec2(origin.x + axis_len, origin.y), IM_COL32(255, 50, 50, 255), 2.0f);
            // Y axis (green)  
            draw_list->AddLine(origin, ImVec2(origin.x, origin.y - axis_len), IM_COL32(50, 255, 50, 255), 2.0f);
            // Z axis (blue)
            draw_list->AddLine(origin, ImVec2(origin.x + axis_len * 0.7f, origin.y + axis_len * 0.7f), IM_COL32(50, 50, 255, 255), 2.0f);
        }
        
        // Demo: Draw some sample objects
        draw_demo_objects(canvas_pos, viewport_size);
        
        // Gizmo placeholder
        if (editor_->get_selection().selected_entity.has_value()) {
            render_gizmo(canvas_pos, viewport_size);
        }
        
        // Update cursor position for proper sizing
        ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x, canvas_pos.y + viewport_size.y));
        ImGui::InvisibleButton("viewport", viewport_size);
        
        ImGui::End();
    }
    
private:
    void draw_demo_objects(ImVec2 pos, ImVec2 size) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        // Ground plane rectangle
        ImVec2 ground_rect_min = ImVec2(pos.x + 100, pos.y + size.y * 0.7f);
        ImVec2 ground_rect_max = ImVec2(pos.x + size.x - 50, pos.y + size.y * 0.7f + 30);
        draw_list->AddRectFilled(ground_rect_min, ground_rect_max, IM_COL32(80, 80, 80, 255));
        
        // Cube (Main Camera / object)
        ImVec2 cube_pos = ImVec2(pos.x + 200, pos.y + size.y * 0.7f - 50);
        ImVec2 cube_size = ImVec2(40, 40);
        draw_list->AddRectFilled(cube_pos, ImVec2(cube_pos.x + cube_size.x, cube_pos.y + cube_size.y), IM_COL32(200, 100, 50, 255));
        
        // Sphere (Player)
        ImVec2 sphere_pos = ImVec2(pos.x + 350, pos.y + size.y * 0.65f - 30);
        draw_list->AddCircleFilled(sphere_pos, 25, IM_COL32(100, 150, 255, 255));
        
        // Capsule-ish (Enemy)
        ImVec2 enemy_pos = ImVec2(pos.x + 500, pos.y + size.y * 0.7f - 45);
        draw_list->AddRectFilled(ImVec2(enemy_pos.x - 15, enemy_pos.y - 30), ImVec2(enemy_pos.x + 15, enemy_pos.y + 30), IM_COL32(200, 50, 50, 255));
        
        // Directional light indicator
        ImVec2 light_pos = ImVec2(pos.x + size.x - 80, pos.y + 50);
        draw_list->AddCircleFilled(light_pos, 15, IM_COL32(255, 255, 100, 255));
        
        // Light rays
        for (int i = 0; i < 5; i++) {
            float angle = 0.3f + i * 0.3f;
            ImVec2 ray_end = ImVec2(
                light_pos.x + cos(angle) * 40,
                light_pos.y + sin(angle) * 40
            );
            draw_list->AddLine(light_pos, ray_end, IM_COL32(255, 255, 100, 100), 1.0f);
        }
    }
    
    void render_gizmo(ImVec2 pos, ImVec2 size) {
        if (editor_->get_gizmo_mode() == GizmoMode::Translate) {
            // Draw translate gizmo at center
            ImVec2 gizmo_origin = ImVec2(pos.x + size.x * 0.4f, pos.y + size.y * 0.5f);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            
            // X axis (red)
            draw_list->AddLine(gizmo_origin, ImVec2(gizmo_origin.x + 30, gizmo_origin.y), IM_COL32(255, 0, 0, 255), 3.0f);
            // Y axis (green)
            draw_list->AddLine(gizmo_origin, ImVec2(gizmo_origin.x, gizmo_origin.y - 30), IM_COL32(0, 255, 0, 255), 3.0f);
            // Z axis (blue)
            draw_list->AddLine(gizmo_origin, ImVec2(gizmo_origin.x + 20, gizmo_origin.y + 20), IM_COL32(0, 0, 255, 255), 3.0f);
        }
    }
    
    Editor* editor_;
    bool show_grid_;
    bool show_axes_;
    int view_mode_idx_ = 0;
};

// ============================================================================
// Console Panel
// ============================================================================

class ConsolePanel {
public:
    ConsolePanel() {
        // Add some demo messages
        messages_.push_back({MessageLevel::Info, "[14:30:15] Editor initialized"});
        messages_.push_back({MessageLevel::Info, "[14:30:16] Loaded scene: demo/scenes/main.scene"});
        messages_.push_back({MessageLevel::Warning, "[14:30:18] Missing texture: assets/ui/missing.png (using default)"});
        messages_.push_back({MessageLevel::Info, "[14:30:20] Entity created: Player"});
        messages_.push_back({MessageLevel::Info, "[14:30:22] Entity created: Main Camera"});
        messages_.push_back({MessageLevel::Error, "[14:30:25] Failed to load behavior: scripts/unknown.nd"});
    }
    
    void render() {
        ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoCollapse);
        
        // Filter buttons
        if (ImGui::Button("Clear")) {
            messages_.clear();
        }
        ImGui::SameLine();
        
        static bool show_info = true;
        static bool show_warnings = true;
        static bool show_errors = true;
        
        ImGui::ToggleButton("Info", &show_info);
        ImGui::SameLine();
        ImGui::ToggleButton("Warnings", &show_warnings);
        ImGui::SameLine();
        ImGui::ToggleButton("Errors", &show_errors);
        
        ImGui::Separator();
        
        // Messages
        if (ImGui::BeginChild("messages")) {
            for (const auto& msg : messages_) {
                bool show = false;
                switch (msg.level) {
                    case MessageLevel::Info: show = show_info; break;
                    case MessageLevel::Warning: show = show_warnings; break;
                    case MessageLevel::Error: show = show_errors; break;
                }
                
                if (!show) continue;
                
                ImVec4 color;
                const char* prefix;
                switch (msg.level) {
                    case MessageLevel::Info:
                        color = ImVec4(1, 1, 1, 1);
                        prefix = "ℹ";
                        break;
                    case MessageLevel::Warning:
                        color = ImVec4(1, 0.8, 0, 1);
                        prefix = "⚠";
                        break;
                    case MessageLevel::Error:
                        color = ImVec4(1, 0.3, 0.3, 1);
                        prefix = "✖";
                        break;
                }
                
                ImGui::TextColored(color, "%s %s", prefix, msg.text.c_str());
            }
            ImGui::EndChild();
        }
        
        ImGui::End();
    }
    
    void add_message(MessageLevel level, const std::string& text) {
        messages_.push_back({level, text});
    }
    
private:
    enum class MessageLevel { Info, Warning, Error };
    struct Message { MessageLevel level; std::string text; };
    std::vector<Message> messages_;
};

// ============================================================================
// AI Assistant Panel
// ============================================================================

class AIAssistantPanel {
public:
    AIAssistantPanel(Editor* editor) : editor_(editor) {}
    
    void render() {
        ImGui::Begin("AI Assistant", nullptr, ImGuiWindowFlags_NoCollapse);
        
        // Header with status
        ImGui::Text("Claude Integration");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "● Connected");
        
        ImGui::Separator();
        
        // Context info
        ImGui::TextDisabled("Context:");
        if (editor_->get_selection().selected_entity.has_value()) {
            ImGui::Text("  Selected: Entity %u", editor_->get_selection().selected_entity->id);
        } else {
            ImGui::Text("  Selected: None");
        }
        
        ImGui::Separator();
        
        // Chat messages
        if (ImGui::BeginChild("chat", ImVec2(0, -60))) {
            // Demo messages
            add_message("How can I improve the lighting in this scene?", false);
            add_message("Consider adding a point light near the player for better atmosphere. I can generate a shader that creates dynamic shadows from the player.", true);
            add_message("Create a new enemy behavior", false);
            add_message("I'll create a patrol behavior for enemies. This will make them follow a set path and alert when detecting the player.", true);
            ImGui::EndChild();
        }
        
        // Input
        ImGui::Separator();
        ImGui::InputText("##prompt", input_buffer_, sizeof(input_buffer_), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Send")) {
            if (strlen(input_buffer_) > 0) {
                add_message(input_buffer_, false);
                input_buffer_[0] = '\0';
                
                // Simulate response
                add_message("Processing your request...", true);
            }
        }
        
        // Quick actions
        ImGui::Separator();
        ImGui::TextDisabled("Quick Actions:");
        if (ImGui::Button("Generate Mesh")) {
            add_message("Generate Mesh - Creating procedural mesh via AI", false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit Behavior")) {
            add_message("Edit Behavior - Opening behavior editor", false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Script")) {
            add_message("Create Script - Writing new behavior script", false);
        }
        
        ImGui::End();
    }
    
private:
    void add_message(const char* text, bool is_ai) {
        ImVec4 color = is_ai ? ImVec4(0.4f, 0.8f, 1.0f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        ImGui::TextColored(color, "%s %s", is_ai ? "🤖" : "👤", text);
    }
    
    Editor* editor_;
    char input_buffer_[512] = "";
};

// ============================================================================
// Editor Implementation
// ============================================================================

Editor::Editor()
    : mode_(EditorMode::Edit)
    , gizmo_mode_(GizmoMode::Translate)
{
}

Editor::~Editor() {
    shutdown();
}

bool Editor::initialize(Engine* engine) {
    engine_ = engine;
    
    // Initialize entity manager
    entity_manager_ = std::make_unique<EntityManager>();
    
    // Initialize panels
    init_panels();
    
    spdlog::info("Editor initialized");
    return true;
}

void Editor::init_panels() {
    menu_bar_ = std::make_unique<MenuBar>();
    toolbar_ = std::make_unique<Toolbar>(this);
    hierarchy_panel_ = std::make_unique<HierarchyPanel>(this);
    inspector_panel_ = std::make_unique<InspectorPanel>(this);
    viewport_panel_ = std::make_unique<ViewportPanel>(this);
    console_panel_ = std::make_unique<ConsolePanel>();
    ai_assistant_panel_ = std::make_unique<AIAssistantPanel>(this);
}

void Editor::shutdown_panels() {
    menu_bar_.reset();
    toolbar_.reset();
    hierarchy_panel_.reset();
    inspector_panel_.reset();
    viewport_panel_.reset();
    console_panel_.reset();
    ai_assistant_panel_.reset();
}

void Editor::update(float delta_time) {
    // Update logic would go here
}

void Editor::render() {
    // Set up dock space
    render_dock_space();
    
    // Render menu and toolbar
    render_menu_bar();
    render_toolbar();
    
    // Render panels
    viewport_panel_->render();
    hierarchy_panel_->render();
    inspector_panel_->render();
    console_panel_->render();
    ai_assistant_panel_->render();
}

void Editor::render_dock_space() {
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    
    // Fullscreen dock space
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse 
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | dockspace_flags;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar();
    
    // Dock builder (would set up initial layout in real implementation)
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    
    ImGui::End();
}

void Editor::render_menu_bar() {
    menu_bar_->render();
}

void Editor::render_toolbar() {
    ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
    toolbar_->render();
    ImGui::End();
}

void Editor::handle_input() {
    // Handle keyboard shortcuts
}

void Editor::shutdown() {
    shutdown_panels();
    spdlog::info("Editor shutdown");
}

void Editor::select_entity(EntityId id) {
    selection_.selected_entity = id;
}

void Editor::clear_selection() {
    selection_.selected_entity.reset();
}

EntityId Editor::create_entity(const std::string& name) {
    // In real implementation, would create via entity manager
    return EntityId{0};
}

void Editor::delete_entity(EntityId id) {
    // Implementation
}

void Editor::duplicate_entity(EntityId id) {
    // Implementation
}

void Editor::log_info(const std::string& msg) {
    console_panel_->add_message(ConsolePanel::MessageLevel::Info, msg);
}

void Editor::log_warning(const std::string& msg) {
    console_panel_->add_message(ConsolePanel::MessageLevel::Warning, msg);
}

void Editor::log_error(const std::string& msg) {
    console_panel_->add_message(ConsolePanel::MessageLevel::Error, msg);
}

std::unique_ptr<Editor> create_editor() {
    return std::make_unique<Editor>();
}

} // namespace editor
} // namespace odyssey
