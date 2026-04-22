#pragma once

#include <vector>
#include <string>

namespace odyssey::editor {

// Built-in layout presets for ImGui docking.
enum class LayoutPreset {
    Default,      // Left 0.20, Right 0.25, Bottom 0.25
    TwoByThree,   // 2x3 grid: left column (Hierarchy/Project), center (Viewport), right (Inspector/Log)
    Tall,         // Deep narrow center with wider Inspector
    Wide          // Low tall center with short Inspector
};

// ImGui docking split instruction: dir, ratio, window names.
struct DockSplit {
    int dir;                    // ImGuiDir_ enum value
    float ratio;                // Split ratio (0.0 to 1.0)
    const char* window_on_split;  // Window to split off
    const char* window_remaining; // Window that remains after split
};

// Returns the split instructions for a given preset.
// Pure: caller applies these via ImGui::DockBuilderSplitNode etc.
std::vector<DockSplit> preset_splits(LayoutPreset p);

// Returns human-readable preset name ("Default", "2-by-3", "Tall", "Wide").
const char* preset_name(LayoutPreset p);

// Reverse lookup: given a name, return the preset. Returns Default if unknown.
LayoutPreset preset_from_name(const char* name);

// Converts a user-supplied layout name to a slug for filesystem use.
// Lowercase ASCII, underscore separators, max 64 chars.
// Empty string → "_unnamed", overlong → truncated.
std::string slug_layout_name(const std::string& name);

} // namespace odyssey::editor
