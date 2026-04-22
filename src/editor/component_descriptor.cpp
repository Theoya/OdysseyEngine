#include "editor/component_descriptor.h"
#include "core/types.h"

using namespace odyssey;

namespace odyssey::editor {

// Predicate helpers — pure checks on Entity component fields.
static bool has_transform(const scene::Entity& e) {
    // Transform is always present (even if identity).
    return true;
}

static bool has_stats(const scene::Entity& e) {
    const auto& s = e.components.stats;
    return s.health > 0 || s.max_health > 0 || s.ammo > 0 || s.speed > 0;
}

static bool has_mesh_renderer(const scene::Entity& e) {
    return !e.components.mesh_path.empty() ||
           !e.components.material_path.empty();
}

static bool has_behavior(const scene::Entity& e) {
    return !e.components.behavior_shader.empty();
}

static bool has_script(const scene::Entity& e) {
    return !e.components.script_class.empty();
}

static bool has_tags(const scene::Entity& e) {
    return !e.components.tags.empty();
}

static bool has_voice_source(const scene::Entity& e) {
    return e.components.voice_range > 0.0f;
}

static bool has_prefab_source(const scene::Entity& e) {
    return !e.components.prefab_source.empty();
}

// Add helpers — initialize components with defaults.
static void add_transform(scene::Entity& e) {
    // Transform already initialized by EntityComponents constructor.
    // This is a no-op for the UI layer.
}

static void add_stats(scene::Entity& e) {
    // Initialize with reasonable defaults if empty. Leave existing values.
    if (e.components.stats.max_health == 0) {
        e.components.stats.max_health = 100.0f;
    }
    if (e.components.stats.health == 0) {
        e.components.stats.health = e.components.stats.max_health;
    }
}

static void add_mesh_renderer(scene::Entity& e) {
    // Mark presence with a placeholder path. The user fills in real paths.
    if (e.components.mesh_path.empty()) {
        e.components.mesh_path = " ";  // Non-empty sentinel
    }
}

static void add_behavior(scene::Entity& e) {
    if (e.components.behavior_shader.empty()) {
        e.components.behavior_shader = " ";
    }
}

static void add_script(scene::Entity& e) {
    if (e.components.script_class.empty()) {
        e.components.script_class = " ";
    }
}

static void add_tags(scene::Entity& e) {
    if (e.components.tags.empty()) {
        e.components.tags.emplace_back("new_tag");
    }
}

static void add_voice_source(scene::Entity& e) {
    if (e.components.voice_range <= 0.0f) {
        e.components.voice_range = 25.0f;
    }
}

static void add_prefab_source(scene::Entity& e) {
    // Prefab source is read-only — no add action.
}

// Remove helpers — clear/zero the component.
static void remove_transform(scene::Entity& e) {
    // Transform is mandatory. This is a no-op.
}

static void remove_stats(scene::Entity& e) {
    e.components.stats = EntityStats{};
}

static void remove_mesh_renderer(scene::Entity& e) {
    e.components.mesh_path.clear();
    e.components.material_path.clear();
}

static void remove_behavior(scene::Entity& e) {
    e.components.behavior_shader.clear();
}

static void remove_script(scene::Entity& e) {
    e.components.script_class.clear();
    e.components.script_config.clear();
}

static void remove_tags(scene::Entity& e) {
    e.components.tags.clear();
}

static void remove_voice_source(scene::Entity& e) {
    e.components.voice_range = 0.0f;
}

static void remove_prefab_source(scene::Entity& e) {
    // Read-only — no removal action.
}

const std::vector<ComponentDescriptor>& all_component_descriptors() {
    static const std::vector<ComponentDescriptor> descriptors = {
        {
            ComponentKind::Transform,
            "Transform",
            has_transform,
            add_transform,
            remove_transform,
            false,  // not removable
        },
        {
            ComponentKind::Stats,
            "Stats",
            has_stats,
            add_stats,
            remove_stats,
            true,
        }, // NOLINTNEXTLINE
        {
            ComponentKind::MeshRenderer,
            "Mesh Renderer",
            has_mesh_renderer,
            add_mesh_renderer,
            remove_mesh_renderer,
            true,
        },
        {
            ComponentKind::Behavior,
            "Behavior (Nadir)",
            has_behavior,
            add_behavior,
            remove_behavior,
            true,
        },
        {
            ComponentKind::Script,
            "Script",
            has_script,
            add_script,
            remove_script,
            true,
        },
        {
            ComponentKind::Tags,
            "Tags",
            has_tags,
            add_tags,
            remove_tags,
            true,
        },
        {
            ComponentKind::VoiceSource,
            "Voice Source",
            has_voice_source,
            add_voice_source,
            remove_voice_source,
            true,
        },
        {
            ComponentKind::PrefabSource,
            "Prefab Source",
            has_prefab_source,
            add_prefab_source,
            remove_prefab_source,
            false,  // not removable, also not addable
        },
    };
    return descriptors;
}

std::vector<ComponentKind> missing_components(const scene::Entity& e) {
    std::vector<ComponentKind> missing;
    for (const auto& desc : all_component_descriptors()) {
        if (!desc.is_present(e)) {
            missing.push_back(desc.kind);
        }
    }
    return missing;
}

} // namespace odyssey::editor
