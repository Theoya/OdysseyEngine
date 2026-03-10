#pragma once
#include "core/types.h"
#include "core/result.h"
#include "scene/entity_manager.h"
#include <string>
#include <filesystem>
#include <unordered_map>

namespace odyssey::scene {

struct PrefabData {
    std::string name;
    int version = 1;
    EntityComponents default_components;

    // Collider info
    std::string collider_type;  // "capsule", "box", "sphere"
    float collider_radius = 0.5f;
    float collider_height = 1.8f;
    bool collider_auto_fit = false;

    // Initial AI state
    std::string initial_ai_state;
};

// Pure: parse prefab XML string
Result<PrefabData> parse_prefab_xml(const std::string& xml_content);

// Impure: load prefab from file
Result<PrefabData> load_prefab_file(const std::filesystem::path& path);

// Prefab registry -- caches loaded prefabs
class PrefabRegistry {
public:
    // Load and cache a prefab
    Result<const PrefabData*> load(const std::filesystem::path& path);

    // Get a cached prefab by name
    const PrefabData* get(const std::string& name) const;

    // Apply a prefab's defaults to an entity
    void apply_prefab(Entity& entity, const PrefabData& prefab) const;

    // Clear cache
    void clear();

private:
    std::unordered_map<std::string, PrefabData> prefabs_;
};

} // namespace odyssey::scene
