#pragma once
#include "core/types.h"
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>
#include <any>
#include <vector>

namespace odyssey::scripting {

// Phase 9: Script execution phases (gates Physics::world_mut access)
enum class ScriptPhase {
    PrePhysics,   // Can write to Rigidbody; script outputs are authoritative
    PostPhysics,  // Read-only on physics state; bodies already stepped
};

class ScriptContext {
public:
    EntityID player_id = INVALID_ENTITY;
    float delta_time = 0.0f;
    float total_time = 0.0f;
    uint32_t frame_number = 0;
    ScriptPhase current_phase = ScriptPhase::PrePhysics;  // Phase 9

    // Key-value store for game state (quest states, flags, counters)
    template<typename T>
    T get(std::string_view key) const {
        auto it = state_store_.find(std::string(key));
        if (it == state_store_.end()) return T{};
        try { return std::any_cast<T>(it->second); }
        catch (...) { return T{}; }
    }

    template<typename T>
    T get(std::string_view key, const T& default_val) const {
        auto it = state_store_.find(std::string(key));
        if (it == state_store_.end()) return default_val;
        try { return std::any_cast<T>(it->second); }
        catch (...) { return default_val; }
    }

    bool has_key(std::string_view key) const;

    // Entity queries (all const, pure reads)
    bool has_item(EntityID entity, std::string_view item) const;
    vec3 get_position(EntityID entity) const;
    float get_health(EntityID entity) const;
    float get_distance(EntityID a, EntityID b) const;
    bool is_alive(EntityID entity) const;

    // Spatial query: find entities within radius
    std::vector<EntityID> entities_in_radius(vec3 center, float radius) const;

    // Find entity by name
    EntityID find_entity(std::string_view name) const;

    // Input state
    bool is_key_pressed(std::string_view key) const;
    bool is_key_just_pressed(std::string_view key) const;

    // --- Builder methods (used by engine to populate context) ---
    void set_state(const std::string& key, std::any value);
    void add_entity_position(EntityID id, vec3 pos);
    void add_entity_health(EntityID id, float health, float max_health);
    void add_entity_name(EntityID id, const std::string& name);
    void add_item(EntityID entity, const std::string& item);
    void set_key_state(const std::string& key, bool pressed, bool just_pressed);
    void clear();

private:
    std::unordered_map<std::string, std::any> state_store_;
    std::unordered_map<EntityID, vec3> entity_positions_;
    std::unordered_map<EntityID, std::pair<float, float>> entity_health_; // health, max_health
    std::unordered_map<EntityID, std::string> entity_names_;
    std::unordered_map<std::string, EntityID> name_to_entity_;
    std::unordered_map<EntityID, std::vector<std::string>> entity_items_;
    std::unordered_map<std::string, std::pair<bool, bool>> key_states_; // pressed, just_pressed
};

} // namespace odyssey::scripting
