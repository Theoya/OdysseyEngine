#include "editor/layout_presets.h"

#include <imgui.h>
#include <cctype>
#include <algorithm>

namespace odyssey::editor {

std::vector<DockSplit> preset_splits(LayoutPreset p) {
    std::vector<DockSplit> splits;

    switch (p) {
    case LayoutPreset::Default: {
        // Default: Left 0.20 (Hierarchy), Right 0.25 (Inspector), Bottom 0.25 (AssetBrowser+Log)
        // Split sequence:
        // 1. Split root right: ratio=0.75, keep left (Editor), split off right (Inspector area)
        // 2. In remaining center, split bottom: ratio=0.75, keep top (Viewport), split off bottom (Panels)
        // 3. In right area, split bottom: ratio=0.50, keep top (Inspector), split off bottom
        splits.push_back({ImGuiDir_Right, 0.80f, "SceneViewport", nullptr});  // Right 20%
        splits.push_back({ImGuiDir_Right, 0.75f, "Inspector", nullptr});      // Right 25% of remaining
        splits.push_back({ImGuiDir_Down, 0.75f, "AssetBrowser", nullptr});    // Bottom 25% of remaining
        break;
    }

    case LayoutPreset::TwoByThree: {
        // 2x3: left column 0.33 (Hierarchy top / Project bottom)
        //      center 0.34 (Viewport)
        //      right 0.33 (Inspector top / Log bottom)
        splits.push_back({ImGuiDir_Right, 0.67f, "SceneViewport", nullptr}); // Right 33%
        splits.push_back({ImGuiDir_Right, 0.50f, "Inspector", nullptr});     // Right 50% of remaining
        splits.push_back({ImGuiDir_Down, 0.50f, "Hierarchy", nullptr});      // Bottom 50% of left
        splits.push_back({ImGuiDir_Down, 0.50f, "Log", nullptr});            // Bottom 50% of right
        break;
    }

    case LayoutPreset::Tall: {
        // Tall: deep narrow center, Inspector wider
        splits.push_back({ImGuiDir_Right, 0.60f, "SceneViewport", nullptr}); // Right 40%
        splits.push_back({ImGuiDir_Down, 0.70f, "AssetBrowser", nullptr});   // Bottom 30% of left
        break;
    }

    case LayoutPreset::Wide: {
        // Wide: low tall center, Inspector short
        splits.push_back({ImGuiDir_Right, 0.75f, "Inspector", nullptr});     // Right 25%
        splits.push_back({ImGuiDir_Down, 0.20f, "Log", nullptr});            // Bottom 20% of center
        break;
    }
    }

    return splits;
}

const char* preset_name(LayoutPreset p) {
    switch (p) {
    case LayoutPreset::Default: return "Default";
    case LayoutPreset::TwoByThree: return "2-by-3";
    case LayoutPreset::Tall: return "Tall";
    case LayoutPreset::Wide: return "Wide";
    }
    return "Unknown";
}

LayoutPreset preset_from_name(const char* name) {
    if (!name) return LayoutPreset::Default;

    if (std::string(name) == "2-by-3") return LayoutPreset::TwoByThree;
    if (std::string(name) == "Tall") return LayoutPreset::Tall;
    if (std::string(name) == "Wide") return LayoutPreset::Wide;

    return LayoutPreset::Default;
}

std::string slug_layout_name(const std::string& name) {
    if (name.empty()) return "_unnamed";

    std::string slug;
    slug.reserve(64);

    for (char c : name) {
        if (slug.size() >= 64) break;

        if (std::isalnum(static_cast<unsigned char>(c))) {
            // Alphanumeric: lowercase
            slug += std::tolower(static_cast<unsigned char>(c));
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            // Spaces → underscore (but avoid leading/trailing/consecutive underscores)
            if (!slug.empty() && slug.back() != '_') {
                slug += '_';
            }
        }
        // Other characters (including non-ASCII, punctuation) are dropped
    }

    // Trim trailing underscores
    while (!slug.empty() && slug.back() == '_') {
        slug.pop_back();
    }

    return slug.empty() ? "_unnamed" : slug;
}

} // namespace odyssey::editor
