#include "editor/inspector_panel.h"
#include "editor/editor.h"
#include "editor/mode_enum.h"
#include "editor/component_descriptor.h"
#include "editor/component_clipboard.h"

#include "core/math_util.h"
#include "scene/entity_manager.h"
#include "scene/scene_loader.h"
#include "scripting/script_registry.h"

#include <imgui.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace odyssey::editor {

InspectorPanel::InspectorPanel() = default;

// ---------------------------------------------------------------------------
// Local helpers — pure, no imgui.
// ---------------------------------------------------------------------------

// Find the EntityDesc in a SceneData that corresponds to the currently-
// selected Entity. Identity match is by `id` (the scene XML attribute) ==
// Entity::name (which scene_loader::populate_entities sets from desc.id).
static scene::SceneData::EntityDesc* find_desc_for_entity(
    scene::SceneData* scene_data, const scene::Entity& entity)
{
    if (!scene_data) return nullptr;
    for (auto& d : scene_data->entities) {
        if (!d.id.empty() && d.id == entity.name) return &d;
    }
    return nullptr;
}

// Load a text file into a string, bounded to `max_bytes`. Used for the
// preview pane; keeps the inspector snappy if someone points it at a huge
// file. Returns "" on read failure.
static std::string read_file_bounded(const std::filesystem::path& p,
                                     size_t max_bytes = 64 * 1024) {
    std::ifstream f(p, std::ios::in | std::ios::binary);
    if (!f.is_open()) return {};
    std::string s;
    s.resize(max_bytes);
    f.read(s.data(), static_cast<std::streamsize>(s.size()));
    s.resize(static_cast<size_t>(f.gcount()));
    return s;
}

static bool is_nadir_keyword(const std::string& w) {
    // Minimal set — enough for visual distinction on .nadir shader files.
    static const char* kw[] = {
        "void","uint","int","float","bool","vec2","vec3","vec4",
        "mat3","mat4","if","else","for","while","return","layout",
        "uniform","buffer","readonly","writeonly","in","out","inout",
        "const","true","false","break","continue","discard",
        "shared","push_constant","local_size_x","local_size_y",
        "std140","std430","gl_GlobalInvocationID","gl_WorkGroupID",
        "gl_LocalInvocationID"
    };
    for (auto* k : kw) if (w == k) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Entity edit widgets — impure (ImGui), small, one responsibility each.
//
// Contract: each widget returns true when the value changed. Callers bubble
// that through to `mutated = true` and the serializer flip.
// ---------------------------------------------------------------------------

static bool edit_vec3_drag(const char* label, vec3& v, float speed,
                           bool read_only) {
    ImGui::PushID(label);
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(140.0f);
    float buf[3] = { v.x, v.y, v.z };
    ImGui::SetNextItemWidth(-1.0f);
    if (read_only) ImGui::BeginDisabled();
    bool changed = ImGui::DragFloat3("##v", buf, speed, 0.0f, 0.0f, "%.3f");
    if (read_only) ImGui::EndDisabled();
    if (changed && !read_only) {
        v.x = buf[0]; v.y = buf[1]; v.z = buf[2];
    }
    ImGui::PopID();
    return changed && !read_only;
}

static bool edit_float_drag(const char* label, float& v, float speed,
                            float lo, float hi, bool read_only) {
    ImGui::PushID(label);
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(-1.0f);
    if (read_only) ImGui::BeginDisabled();
    bool changed = ImGui::DragFloat("##f", &v, speed, lo, hi, "%.3f");
    if (read_only) ImGui::EndDisabled();
    ImGui::PopID();
    return changed && !read_only;
}

// Tooltip helper: hover a just-rendered item and show a read-only hint when
// we're not in Edit mode. Keeps the Play/Simulate-mode gating discoverable.
static void hint_read_only_on_hover(bool read_only) {
    if (!read_only) return;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Switch to Edit mode to modify.");
    }
}

// ---------------------------------------------------------------------------
// Asset preview panel — shown when EditorState::selected_asset is non-empty.
// ---------------------------------------------------------------------------

static void draw_plain_text(const std::string& text) {
    // Bounded TextUnformatted — ImGui handles wrapping.
    ImGui::BeginChild("##preview_text", ImVec2(0, 0),
                      ImGuiChildFlags_Border);
    ImGui::PushTextWrapPos(0.0f);
    if (text.empty()) {
        ImGui::TextDisabled("(empty file or read error)");
    } else {
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
}

static void draw_nadir_colored(const std::string& text) {
    // Extremely simple "syntax highlight": tokenize on whitespace/punctuation
    // and color known keywords. Not a real parser — good enough for a
    // Phase 4 preview. No caret, no editing.
    const ImVec4 kw_col  = ImVec4(0.70f, 0.85f, 1.0f, 1.0f);
    const ImVec4 num_col = ImVec4(0.95f, 0.80f, 0.50f, 1.0f);
    const ImVec4 cmt_col = ImVec4(0.55f, 0.65f, 0.55f, 1.0f);
    const ImVec4 def_col = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

    ImGui::BeginChild("##preview_nadir", ImVec2(0, 0),
                      ImGuiChildFlags_Border);
    std::string line;
    auto flush_line = [&](const std::string& L) {
        if (L.empty()) { ImGui::NewLine(); return; }
        // Comment?
        size_t cmt = L.find("//");
        std::string code = (cmt != std::string::npos) ? L.substr(0, cmt) : L;
        std::string cmt_tail = (cmt != std::string::npos) ? L.substr(cmt) : "";

        std::string tok;
        for (size_t i = 0; i <= code.size(); ++i) {
            char c = (i < code.size()) ? code[i] : '\0';
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                tok.push_back(c);
            } else {
                if (!tok.empty()) {
                    bool all_num = true;
                    for (char ch : tok) {
                        if (!std::isdigit(static_cast<unsigned char>(ch)) &&
                            ch != '.' && ch != 'f') { all_num = false; break; }
                    }
                    ImVec4 tcol = is_nadir_keyword(tok) ? kw_col
                                 : (all_num ? num_col : def_col);
                    ImGui::TextColored(tcol, "%s", tok.c_str());
                    ImGui::SameLine(0.0f, 0.0f);
                    tok.clear();
                }
                if (c != '\0') {
                    char one[2] = { c, 0 };
                    ImGui::TextColored(def_col, "%s", one);
                    ImGui::SameLine(0.0f, 0.0f);
                }
            }
        }
        if (!cmt_tail.empty()) {
            ImGui::TextColored(cmt_col, "%s", cmt_tail.c_str());
        } else {
            ImGui::NewLine();
        }
    };

    for (char c : text) {
        if (c == '\n') { flush_line(line); line.clear(); }
        else if (c != '\r') line.push_back(c);
    }
    if (!line.empty()) flush_line(line);
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Component card helper — renders a collapsible header with hamburger menu
// ---------------------------------------------------------------------------

static void draw_component_card_header(
    const char* label,
    bool removable,
    bool& should_remove,
    bool& should_reset,
    bool& should_copy,
    bool& should_paste)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
                              ImGuiTreeNodeFlags_AllowItemOverlap;
    bool is_open = ImGui::CollapsingHeader(label, flags);

    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    ImGui::PushID(label);
    if (ImGui::SmallButton("...")) {
        ImGui::OpenPopup("component_menu");
    }
    ImGui::PopID();

    if (ImGui::BeginPopup("component_menu")) {
        if (removable && ImGui::MenuItem("Remove")) {
            should_remove = true;
        }
        if (ImGui::MenuItem("Reset")) {
            should_reset = true;
        }
        if (ImGui::MenuItem("Copy Values")) {
            should_copy = true;
        }
        if (ImGui::MenuItem("Paste Values")) {
            should_paste = true;
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Main draw
// ---------------------------------------------------------------------------

void InspectorPanel::draw(EditorState& state) {
    if (!visible_) return;

    if (!ImGui::Begin(name_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    const bool read_only = (state.mode != Mode::Edit);

    // --- Asset preview mode (Phase 4) ---
    if (!state.selected_asset.empty()) {
        if (ImGui::SmallButton("Back to entity")) {
            state.selected_asset.clear();
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1.0f), "%s",
                           state.selected_asset.generic_string().c_str());
        ImGui::Separator();

        if (preview_path_ != state.selected_asset.generic_string()) {
            preview_path_     = state.selected_asset.generic_string();
            preview_text_     = read_file_bounded(state.selected_asset);
            const std::string p = state.selected_asset.generic_string();
            preview_is_nadir_ = (p.size() >= 6 &&
                                 p.compare(p.size() - 6, 6, ".nadir") == 0);
        }
        if (preview_is_nadir_) draw_nadir_colored(preview_text_);
        else                   draw_plain_text(preview_text_);

        ImGui::End();
        return;
    }

    // --- Entity inspector mode ---
    if (!state.entities || state.selected_entity == INVALID_ENTITY) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::Separator();
        ImGui::TextWrapped(
            "Click an entity in the Scene Tree to view its components, or "
            "click an asset in the Asset Browser to preview it.");
        ImGui::End();
        return;
    }

    scene::Entity* e = state.entities->get_entity(state.selected_entity);
    if (!e) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                           "Entity %u not found (stale selection).",
                           state.selected_entity);
        ImGui::End();
        return;
    }
    auto* scene_data = static_cast<scene::SceneData*>(state.scene_data);
    auto* desc = find_desc_for_entity(scene_data, *e);

    // --- Debug toggle ---
    bool debug_mode = false;  // TODO: persist in EditorState if needed
    ImGui::Checkbox("Debug", &debug_mode);
    ImGui::SameLine();

    // Identity
    ImGui::TextColored(ImVec4(0.85f, 0.9f, 1.0f, 1), "%s", e->name.c_str());
    ImGui::TextDisabled("archetype=%s  active=%s  mode=%s",
                        e->archetype.c_str(),
                        e->active ? "yes" : "no",
                        std::string{mode_label(state.mode)}.c_str());
    if (debug_mode) {
        ImGui::TextDisabled("id=%u", e->id);
    }
    if (read_only) {
        ImGui::TextColored(ImVec4(1.0f, 0.70f, 0.45f, 1.0f),
                           "[read-only — switch to Edit mode to modify]");
    }
    ImGui::Separator();

    auto mark_mutated = [&] {
        if (scene_data) scene_data->mutated = true;
    };

    auto& clipboard = component_clipboard();

    // --- Transform card ---
    {
        bool should_remove = false, should_reset = false;
        bool should_copy = false, should_paste = false;
        draw_component_card_header("Transform", false, should_remove, should_reset,
                                   should_copy, should_paste);

        auto& t = e->components.transform;

        if (should_copy) {
            clipboard.transform = t;
        }
        if (should_paste && clipboard.transform) {
            t = *clipboard.transform;
            if (desc) desc->transform = t;
            mark_mutated();
        }
        if (should_reset) {
            t = Transform{};
            if (desc) desc->transform = t;
            mark_mutated();
        }

        if (edit_vec3_drag("position", t.position, 0.1f, read_only)) {
            if (desc) desc->transform.position = t.position;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);

        vec3 euler = quat_to_euler_xyz_deg(t.rotation);
        if (edit_vec3_drag("rotation (deg)", euler, 0.5f, read_only)) {
            t.rotation = euler_xyz_deg_to_quat(euler);
            if (desc) desc->transform.rotation = t.rotation;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);

        if (edit_vec3_drag("scale", t.scale, 0.05f, read_only)) {
            if (desc) desc->transform.scale = t.scale;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);
    }

    // --- Stats card ---
    if (e->components.stats.health > 0 || e->components.stats.max_health > 0 ||
        e->components.stats.ammo > 0 || e->components.stats.speed > 0) {
        bool should_remove = false, should_reset = false;
        bool should_copy = false, should_paste = false;
        draw_component_card_header("Stats", true, should_remove, should_reset,
                                   should_copy, should_paste);

        auto& s = e->components.stats;

        if (should_copy) {
            clipboard.stats = s;
        }
        if (should_paste && clipboard.stats) {
            s = *clipboard.stats;
            if (desc) desc->stats = s;
            mark_mutated();
        }
        if (should_reset) {
            s = EntityStats{};
            if (desc) desc->stats = s;
            mark_mutated();
        }
        if (should_remove) {
            s = EntityStats{};
            if (desc) desc->stats = s;
            mark_mutated();
        }

        if (edit_float_drag("health", s.health, 1.0f, 0.0f, 99999.0f, read_only)) {
            if (desc) desc->stats.health = s.health;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);
        if (edit_float_drag("max_health", s.max_health, 1.0f, 0.0f, 99999.0f, read_only)) {
            if (desc) desc->stats.max_health = s.max_health;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);
        if (edit_float_drag("ammo", s.ammo, 1.0f, 0.0f, 99999.0f, read_only)) {
            if (desc) desc->stats.ammo = s.ammo;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);
        if (edit_float_drag("speed", s.speed, 0.1f, 0.0f, 999.0f, read_only)) {
            if (desc) desc->stats.speed = s.speed;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);
    }

    // --- Mesh Renderer card ---
    if (!e->components.mesh_path.empty() || !e->components.material_path.empty()) {
        bool should_remove = false, should_reset = false;
        bool should_copy = false, should_paste = false;
        draw_component_card_header("Mesh Renderer", true, should_remove, should_reset,
                                   should_copy, should_paste);

        if (should_copy) {
            clipboard.mesh_path = e->components.mesh_path;
            clipboard.material_path = e->components.material_path;
        }
        if (should_paste) {
            if (clipboard.mesh_path) {
                e->components.mesh_path = *clipboard.mesh_path;
                if (desc) desc->mesh_src = e->components.mesh_path;
            }
            if (clipboard.material_path) {
                e->components.material_path = *clipboard.material_path;
                if (desc) desc->material_src = e->components.material_path;
            }
            mark_mutated();
        }
        if (should_reset) {
            e->components.mesh_path.clear();
            e->components.material_path.clear();
            if (desc) {
                desc->mesh_src.clear();
                desc->material_src.clear();
            }
            mark_mutated();
        }
        if (should_remove) {
            e->components.mesh_path.clear();
            e->components.material_path.clear();
            if (desc) {
                desc->mesh_src.clear();
                desc->material_src.clear();
            }
            mark_mutated();
        }

        ImGui::TextDisabled("mesh"); ImGui::SameLine(140.0f);
        ImGui::Text("%s", e->components.mesh_path.empty() ? "<none>"
                                                          : e->components.mesh_path.c_str());

        // F26 drag-drop target for mesh
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MESH")) {
                const char* path = static_cast<const char*>(payload->Data);
                e->components.mesh_path = std::string(path, payload->DataSize);
                if (desc) desc->mesh_src = e->components.mesh_path;
                mark_mutated();
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::TextDisabled("material"); ImGui::SameLine(140.0f);
        ImGui::Text("%s", e->components.material_path.empty() ? "<none>"
                                                              : e->components.material_path.c_str());

        // F26 drag-drop target for material
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MATERIAL")) {
                const char* path = static_cast<const char*>(payload->Data);
                e->components.material_path = std::string(path, payload->DataSize);
                if (desc) desc->material_src = e->components.material_path;
                mark_mutated();
            }
            ImGui::EndDragDropTarget();
        }
    }

    // --- Behavior card ---
    if (!e->components.behavior_shader.empty()) {
        bool should_remove = false, should_reset = false;
        bool should_copy = false, should_paste = false;
        draw_component_card_header("Behavior (Nadir)", true, should_remove,
                                   should_reset, should_copy, should_paste);

        if (should_copy) {
            clipboard.behavior_shader = e->components.behavior_shader;
        }
        if (should_paste && clipboard.behavior_shader) {
            e->components.behavior_shader = *clipboard.behavior_shader;
            if (desc) desc->behavior_shader = e->components.behavior_shader;
            mark_mutated();
        }
        if (should_reset) {
            e->components.behavior_shader.clear();
            if (desc) desc->behavior_shader.clear();
            mark_mutated();
        }
        if (should_remove) {
            e->components.behavior_shader.clear();
            if (desc) desc->behavior_shader.clear();
            mark_mutated();
        }

        ImGui::TextDisabled("shader"); ImGui::SameLine(140.0f);
        ImGui::Text("%s", e->components.behavior_shader.c_str());

        // F26 drag-drop target for behavior
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BEHAVIOR")) {
                const char* path = static_cast<const char*>(payload->Data);
                e->components.behavior_shader = std::string(path, payload->DataSize);
                if (desc) desc->behavior_shader = e->components.behavior_shader;
                mark_mutated();
            }
            ImGui::EndDragDropTarget();
        }
    }

    // --- Script card (F30) ---
    if (!e->components.script_class.empty()) {
        bool should_remove = false, should_reset = false;
        bool should_copy = false, should_paste = false;
        draw_component_card_header("Script", true, should_remove, should_reset,
                                   should_copy, should_paste);

        if (should_copy) {
            clipboard.script_class = e->components.script_class;
            clipboard.script_config = e->components.script_config;
        }
        if (should_paste) {
            if (clipboard.script_class) {
                e->components.script_class = *clipboard.script_class;
                if (desc) desc->script_class = e->components.script_class;
            }
            if (clipboard.script_config) {
                e->components.script_config = *clipboard.script_config;
                if (desc) desc->script_config = e->components.script_config;
            }
            mark_mutated();
        }
        if (should_reset) {
            e->components.script_class.clear();
            e->components.script_config.clear();
            if (desc) {
                desc->script_class.clear();
                desc->script_config.clear();
            }
            mark_mutated();
        }
        if (should_remove) {
            e->components.script_class.clear();
            e->components.script_config.clear();
            if (desc) {
                desc->script_class.clear();
                desc->script_config.clear();
            }
            mark_mutated();
        }

        ImGui::TextDisabled("class"); ImGui::SameLine(140.0f);
        ImGui::Text("%s", e->components.script_class.c_str());
        if (!read_only) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Detach")) {
                e->components.script_class.clear();
                e->components.script_config.clear();
                if (desc) {
                    desc->script_class.clear();
                    desc->script_config.clear();
                }
                mark_mutated();
            }
        }

        ImGui::TextDisabled("config"); ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(-1.0f);
        char config_buf[2048];
        std::snprintf(config_buf, sizeof(config_buf), "%s",
                     e->components.script_config.c_str());
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
        if (read_only) flags |= ImGuiInputTextFlags_ReadOnly;
        if (ImGui::InputTextMultiline("##script_config", config_buf, sizeof(config_buf),
                                       ImVec2(0, 80), flags)) {
            e->components.script_config = config_buf;
            if (desc) desc->script_config = e->components.script_config;
            mark_mutated();
        }
    }

    // --- Tags card ---
    if (!e->components.tags.empty()) {
        bool should_remove = false, should_reset = false;
        bool should_copy = false, should_paste = false;
        draw_component_card_header("Tags", true, should_remove, should_reset,
                                   should_copy, should_paste);

        auto& tags = e->components.tags;

        if (should_copy) {
            clipboard.tags = tags;
        }
        if (should_paste && clipboard.tags) {
            tags = *clipboard.tags;
            if (desc) desc->tags = tags;
            mark_mutated();
        }
        if (should_reset) {
            tags.clear();
            if (desc) desc->tags = tags;
            mark_mutated();
        }
        if (should_remove) {
            tags.clear();
            if (desc) desc->tags = tags;
            mark_mutated();
        }

        int remove_idx = -1;
        for (size_t i = 0; i < tags.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s", tags[i].c_str());
            ImGui::SetNextItemWidth(220.0f);
            ImGuiInputTextFlags itflags = ImGuiInputTextFlags_None;
            if (read_only) itflags |= ImGuiInputTextFlags_ReadOnly;
            if (ImGui::InputText("##tag", buf, sizeof(buf), itflags)) {
                tags[i] = buf;
                if (desc) desc->tags = tags;
                mark_mutated();
            }
            hint_read_only_on_hover(read_only);
            ImGui::SameLine();
            if (!read_only) {
                if (ImGui::SmallButton("Remove")) remove_idx = static_cast<int>(i);
            } else {
                ImGui::TextDisabled("(locked)");
            }
            ImGui::PopID();
        }
        if (remove_idx >= 0) {
            tags.erase(tags.begin() + remove_idx);
            if (desc) desc->tags = tags;
            mark_mutated();
        }
        if (!read_only) {
            if (ImGui::Button("Add Tag")) {
                tags.emplace_back("");
                if (desc) desc->tags = tags;
                mark_mutated();
            }
        } else {
            ImGui::TextDisabled("(switch to Edit mode to add tags)");
        }
    }

    // --- Voice Source card ---
    if (e->components.voice_range > 0.0f) {
        bool should_remove = false, should_reset = false;
        bool should_copy = false, should_paste = false;
        draw_component_card_header("Voice Source", true, should_remove, should_reset,
                                   should_copy, should_paste);

        if (should_copy) {
            clipboard.voice_range = e->components.voice_range;
        }
        if (should_paste && clipboard.voice_range) {
            e->components.voice_range = *clipboard.voice_range;
            if (desc) desc->voice_range = e->components.voice_range;
            mark_mutated();
        }
        if (should_reset) {
            e->components.voice_range = 25.0f;
            if (desc) desc->voice_range = e->components.voice_range;
            mark_mutated();
        }
        if (should_remove) {
            e->components.voice_range = 0.0f;
            if (desc) desc->voice_range = 0.0f;
            mark_mutated();
        }

        if (edit_float_drag("voice_range (m)", e->components.voice_range,
                            0.25f, 0.0f, 50.0f, read_only)) {
            if (desc) desc->voice_range = e->components.voice_range;
            mark_mutated();
        }
        hint_read_only_on_hover(read_only);
    }

    // --- Prefab Source card (read-only) ---
    if (!e->components.prefab_source.empty()) {
        ImGui::CollapsingHeader("Prefab Source", ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::TextDisabled("source"); ImGui::SameLine(140.0f);
        ImGui::Text("%s", e->components.prefab_source.c_str());
    }

    ImGui::Separator();

    // --- Add Component button (F25) ---
    if (!read_only) {
        if (ImGui::Button("+ Add Component")) {
            ImGui::OpenPopup("add_component_menu");
        }

        if (ImGui::BeginPopup("add_component_menu")) {
            static char search_buf[128] = {};
            ImGui::InputText("Search##add_comp", search_buf, sizeof(search_buf));

            const auto& descriptors = all_component_descriptors();
            for (const auto& comp_desc : descriptors) {
                // Skip Transform and PrefabSource (not addable)
                if (comp_desc.kind == ComponentKind::Transform ||
                    comp_desc.kind == ComponentKind::PrefabSource) {
                    continue;
                }

                // Skip if already present
                if (comp_desc.is_present(*e)) {
                    continue;
                }

                // Filter by search
                if (search_buf[0] != '\0') {
                    std::string label_lower = comp_desc.display_name;
                    std::string search_lower = search_buf;
                    std::transform(label_lower.begin(), label_lower.end(),
                                  label_lower.begin(), ::tolower);
                    std::transform(search_lower.begin(), search_lower.end(),
                                  search_lower.begin(), ::tolower);
                    if (label_lower.find(search_lower) == std::string::npos) {
                        continue;
                    }
                }

                if (ImGui::MenuItem(comp_desc.display_name.c_str())) {
                    // Make a copy of the descriptor to call add
                    const auto& descs = all_component_descriptors();
                    auto add_it = std::find_if(descs.begin(), descs.end(),
                        [&](const ComponentDescriptor& d) {
                            return d.kind == comp_desc.kind;
                        });
                    if (add_it != descs.end()) {
                        add_it->add(*e);
                        if (desc) {
                            // Mirror to SceneData
                            if (comp_desc.kind == ComponentKind::Stats) {
                                desc->stats = e->components.stats;
                            } else if (comp_desc.kind == ComponentKind::MeshRenderer) {
                                desc->mesh_src = e->components.mesh_path;
                                desc->material_src = e->components.material_path;
                            } else if (comp_desc.kind == ComponentKind::Behavior) {
                                desc->behavior_shader = e->components.behavior_shader;
                            } else if (comp_desc.kind == ComponentKind::Script) {
                                desc->script_class = e->components.script_class;
                                desc->script_config = e->components.script_config;
                            } else if (comp_desc.kind == ComponentKind::Tags) {
                                desc->tags = e->components.tags;
                            } else if (comp_desc.kind == ComponentKind::VoiceSource) {
                                desc->voice_range = e->components.voice_range;
                            }
                        }
                        mark_mutated();
                    }
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
    }

    ImGui::Separator();

    // Save button — available in Edit mode only, or greyed out.
    if (!read_only) {
        if (ImGui::Button("Save Scene")) {
            state.save_requested = true;
        }
        ImGui::SameLine();
        if (scene_data && scene_data->mutated) {
            ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f),
                               "unsaved changes");
        } else {
            ImGui::TextDisabled("no unsaved changes");
        }
    } else {
        ImGui::TextDisabled("Save Scene — switch to Edit mode");
    }

    ImGui::End();
}

} // namespace odyssey::editor
