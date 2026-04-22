#pragma once

#include "core/types.h"
#include <optional>
#include <string>
#include <vector>

namespace odyssey::editor {

// Session clipboard for component copy/paste. A single clipboard holds
// optional fields for each component type. When the user copies a component,
// its values populate the corresponding optional. When pasting, we overwrite
// the entity's fields with the clipboard values (if set).
struct ComponentClipboard {
    std::optional<Transform>           transform;
    std::optional<EntityStats>         stats;
    std::optional<std::string>         mesh_path;
    std::optional<std::string>         material_path;
    std::optional<std::string>         behavior_shader;
    std::optional<std::string>         script_class;
    std::optional<std::string>         script_config;
    std::optional<std::vector<std::string>> tags;
    std::optional<float>               voice_range;
};

// Returns the global session clipboard. Mutable singleton; same instance
// persists across all inspector edits in a session. Impure (singleton
// accessor), but the data itself is pure.
ComponentClipboard& component_clipboard();

} // namespace odyssey::editor
