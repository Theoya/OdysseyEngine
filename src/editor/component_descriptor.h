#pragma once

#include "scene/entity_manager.h"
#include <functional>
#include <string>
#include <vector>

namespace odyssey::editor {

// Classification of component types available in the Inspector.
enum class ComponentKind {
    Transform,
    Stats,
    MeshRenderer,
    Behavior,
    Script,
    Tags,
    VoiceSource,
    PrefabSource,
};

// Descriptor for a single component type. Provides predicates for checking
// presence, adding, removing, and metadata for the UI. Pure data structure
// — contains function objects that implement the component logic.
struct ComponentDescriptor {
    ComponentKind kind;
    std::string display_name;

    // Returns true if the entity has a non-trivial value for this component.
    std::function<bool(const scene::Entity&)> is_present;

    // Initializes the component with default or placeholder values.
    std::function<void(scene::Entity&)> add;

    // Clears/zeroes the component's fields.
    std::function<void(scene::Entity&)> remove;

    // If false, Remove is disabled in the UI (e.g., Transform).
    bool removable = true;
};

// Returns the canonical list of all 8 component descriptors in UI order.
// Pure: returns a static list.
const std::vector<ComponentDescriptor>& all_component_descriptors();

// Returns the set of component kinds that the entity does NOT have.
// Useful for "Add Component" popups. Pure: examines entity state.
std::vector<ComponentKind> missing_components(const scene::Entity& e);

} // namespace odyssey::editor
