#include "editor/asset_browser_panel.h"
#include "editor/editor.h"
#include "editor/mode_enum.h"

#include <imgui.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace odyssey::editor {

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

// Normalize a path to a forward-slash string for classification matches.
// This keeps classify_asset() cross-path-style stable — e.g. a "music/"
// segment check works whether the OS used '\' or '/'.
static std::string forward_slash_string(const std::filesystem::path& p) {
    std::string s = p.generic_string();
    return s;
}

// Lowercase copy (ASCII only — path names are authored ASCII in this repo).
static std::string lower_ascii(std::string s) {
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

// True if `s` ends with `suffix` (case-insensitive).
static bool ends_with_ci(const std::string& s, const char* suffix) {
    const size_t n = std::strlen(suffix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = s[s.size() - n + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

AssetType classify_asset(const std::filesystem::path& p) {
    const std::string s = lower_ascii(forward_slash_string(p));

    // Double-extensions first — they're unambiguous.
    if (ends_with_ci(s, ".scene.xml"))    return AssetType::Scene;
    if (ends_with_ci(s, ".prefab.xml"))   return AssetType::Prefab;
    if (ends_with_ci(s, ".mesh.xml"))     return AssetType::Mesh;
    if (ends_with_ci(s, ".mat.xml"))      return AssetType::Material;
    if (ends_with_ci(s, ".music.xml"))    return AssetType::Music;
    if (ends_with_ci(s, ".actions.xml"))  return AssetType::Actions;
    if (ends_with_ci(s, ".skeleton.xml")) return AssetType::Skeleton;
    if (ends_with_ci(s, ".anim.xml"))     return AssetType::Animation;

    // Nadir behaviors.
    if (ends_with_ci(s, ".nadir")) return AssetType::Behavior;

    // Single-extension XML: disambiguate by directory segment.
    if (ends_with_ci(s, ".xml")) {
        if (s.find("/lighting_profiles/") != std::string::npos ||
            s.find("lighting_profiles/") == 0) {
            return AssetType::LightingProfile;
        }
        if (s.find("/music/") != std::string::npos ||
            s.find("music/") == 0) {
            // Leitmotifs / stinger tables etc. living under music/
            return AssetType::Music;
        }
        return AssetType::Other;
    }
    return AssetType::Other;
}

const char* asset_type_group_label(AssetType t) {
    switch (t) {
    case AssetType::Mesh:            return "Meshes";
    case AssetType::Material:        return "Materials";
    case AssetType::Prefab:          return "Prefabs";
    case AssetType::Behavior:        return "Behaviors (.nadir)";
    case AssetType::LightingProfile: return "Lighting Profiles";
    case AssetType::Music:           return "Music";
    case AssetType::Scene:           return "Scenes";
    case AssetType::Actions:         return "Actions";
    case AssetType::Skeleton:        return "Skeletons";
    case AssetType::Animation:       return "Animations";
    case AssetType::Other:           return "Other";
    }
    return "Other";
}

int asset_type_order_key(AssetType t) {
    // UI order: visual primitives first, then authoring building blocks,
    // then scripts-of-behavior, then DCC-adjacent data.
    switch (t) {
    case AssetType::Mesh:            return 0;
    case AssetType::Material:        return 1;
    case AssetType::Prefab:          return 2;
    case AssetType::Behavior:        return 3;
    case AssetType::LightingProfile: return 4;
    case AssetType::Music:           return 5;
    case AssetType::Scene:           return 6;
    case AssetType::Actions:         return 7;
    case AssetType::Skeleton:        return 8;
    case AssetType::Animation:       return 9;
    case AssetType::Other:           return 99;
    }
    return 99;
}

void sort_assets_canonical(std::vector<AssetEntry>& entries) {
    std::sort(entries.begin(), entries.end(),
        [](const AssetEntry& a, const AssetEntry& b) {
            const int ka = asset_type_order_key(a.type);
            const int kb = asset_type_order_key(b.type);
            if (ka != kb) return ka < kb;
            return a.relative.generic_string() < b.relative.generic_string();
        });
}

std::vector<AssetEntry> enumerate_project(const std::filesystem::path& root) {
    std::vector<AssetEntry> out;
    std::error_code ec;
    if (root.empty() || !std::filesystem::exists(root, ec) ||
        !std::filesystem::is_directory(root, ec)) {
        return out;
    }
    // recursive_directory_iterator — Phase 4 stays simple. Hot reload /
    // file-watching is explicitly deferred (see decision record).
    auto it  = std::filesystem::recursive_directory_iterator(root,
                   std::filesystem::directory_options::skip_permission_denied, ec);
    auto end = std::filesystem::recursive_directory_iterator{};
    for (; !ec && it != end; it.increment(ec)) {
        const auto& entry = *it;
        if (!entry.is_regular_file(ec)) continue;
        AssetEntry e;
        e.path       = entry.path();
        e.relative   = std::filesystem::relative(entry.path(), root, ec);
        if (ec) { ec.clear(); e.relative = entry.path().filename(); }
        e.type       = classify_asset(entry.path());
        e.size_bytes = entry.file_size(ec);
        if (ec) { ec.clear(); e.size_bytes = 0; }
        // Skip "Other" in the canonical list to keep the panel tidy —
        // tests for classification accept Other via direct classify_asset
        // calls, but the browser UI focuses on known authoring types.
        if (e.type == AssetType::Other) continue;
        out.push_back(std::move(e));
    }
    sort_assets_canonical(out);
    return out;
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

AssetBrowserPanel::AssetBrowserPanel() = default;

// Placeholder icon: a small colored square rendered via ImGui::ColorButton.
// Deliberately not a texture — texture-based thumbnails (thumbnail-bake)
// are a later phase. The palette is contrast-balanced against the editor's
// dark WindowBg and is consistent within the mood (no saturated primaries
// clashing with the liminal clear color).
static ImVec4 icon_color_for(AssetType t) {
    switch (t) {
    case AssetType::Mesh:            return ImVec4(0.35f, 0.55f, 0.95f, 1.0f); // blue
    case AssetType::Material:        return ImVec4(0.95f, 0.78f, 0.25f, 1.0f); // gold
    case AssetType::Prefab:          return ImVec4(0.35f, 0.85f, 0.40f, 1.0f); // green
    case AssetType::Behavior:        return ImVec4(0.85f, 0.35f, 0.85f, 1.0f); // magenta
    case AssetType::LightingProfile: return ImVec4(0.95f, 0.90f, 0.65f, 1.0f); // pale yellow
    case AssetType::Music:           return ImVec4(0.55f, 0.65f, 0.90f, 1.0f); // periwinkle
    case AssetType::Scene:           return ImVec4(0.90f, 0.55f, 0.35f, 1.0f); // amber
    case AssetType::Actions:         return ImVec4(0.75f, 0.55f, 0.45f, 1.0f); // tan
    case AssetType::Skeleton:        return ImVec4(0.75f, 0.80f, 0.85f, 1.0f); // bone
    case AssetType::Animation:       return ImVec4(0.55f, 0.85f, 0.85f, 1.0f); // cyan
    case AssetType::Other:           return ImVec4(0.50f, 0.50f, 0.55f, 1.0f); // neutral
    }
    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
}

void AssetBrowserPanel::refresh(const std::filesystem::path& root) {
    entries_  = enumerate_project(root);
    last_root_ = root;
    refreshed_once_ = true;
    spdlog::info("[editor] asset browser: enumerated {} assets under '{}'",
                 entries_.size(), root.string());
}

void AssetBrowserPanel::draw(EditorState& state) {
    if (!visible_) return;
    if (!ImGui::Begin(name_.c_str(), &visible_)) {
        ImGui::End();
        return;
    }

    // Lazy-refresh: first draw, or when the project root changes.
    if (!refreshed_once_ || state.project_root != last_root_) {
        refresh(state.project_root);
    }

    // Header row: root path + Refresh button.
    ImGui::TextDisabled("Root: %s", state.project_root.generic_string().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        refresh(state.project_root);
    }
    ImGui::TextDisabled("%zu assets", entries_.size());
    ImGui::Separator();

    // Walk entries grouped by AssetType. The vector is already canonical-
    // sorted, so we just emit a tree node each time the type changes.
    AssetType current_group = AssetType::Other;
    bool      group_open    = false;
    bool      first_group   = true;

    auto close_group = [&] {
        if (group_open) { ImGui::TreePop(); group_open = false; }
    };

    for (const auto& e : entries_) {
        if (first_group || e.type != current_group) {
            close_group();
            current_group = e.type;
            first_group = false;
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;
            group_open = ImGui::TreeNodeEx(asset_type_group_label(e.type), flags);
        }
        if (!group_open) continue;

        // Icon — a small color square. ColorButton with no-tooltip flag.
        ImGui::PushID(e.relative.generic_string().c_str());
        ImVec4 col = icon_color_for(e.type);
        ImGui::ColorButton("##icon", col,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
            ImVec2(12, 12));
        ImGui::SameLine();

        const bool is_selected = (state.selected_asset == e.path);
        const std::string rel_s = e.relative.generic_string();
        ImGuiSelectableFlags sflags = ImGuiSelectableFlags_AllowDoubleClick;
        if (ImGui::Selectable(rel_s.c_str(), is_selected, sflags)) {
            state.selected_asset = e.path;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            // Scene double-click → request scene swap (gated on Edit).
            if (e.type == AssetType::Scene) {
                if (state.mode == Mode::Edit) {
                    state.scene_swap_request = e.path;
                    spdlog::info("[editor] asset browser: requesting scene swap to '{}'",
                                 e.path.string());
                } else {
                    spdlog::warn("[editor] asset browser: scene swap refused — "
                                 "editor is not in Edit mode (current={})",
                                 mode_label(state.mode));
                }
            } else if (e.type == AssetType::Prefab) {
                // Phase 5+: spawn-from-prefab will land here.
                spdlog::info("[editor] asset browser: prefab double-click is a "
                             "no-op in Phase 4 (spawn-from-prefab is Phase 5+)");
            }
        }
        ImGui::PopID();
    }
    close_group();

    ImGui::End();
}

} // namespace odyssey::editor
