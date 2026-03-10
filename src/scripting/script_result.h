#pragma once
#include "core/types.h"
#include <string>
#include <vector>
#include <any>
#include <variant>

namespace odyssey::scripting {

// Individual mutation types
struct SetStateMutation {
    std::string key;
    std::any value;
};

struct SpawnEntityMutation {
    std::string prefab;
    vec3 position{0.f};
    std::string name;
};

struct DestroyEntityMutation {
    EntityID entity_id;
};

struct TriggerDialogueMutation {
    std::string dialogue_id;
    EntityID speaker = INVALID_ENTITY;
};

struct PlaySoundMutation {
    std::string sound_id;
    vec3 position{0.f};
    float volume = 1.0f;
};

struct SetTransformMutation {
    EntityID entity_id;
    Transform transform;
};

struct DamageEntityMutation {
    EntityID target;
    float amount;
    EntityID source = INVALID_ENTITY;
};

struct HealEntityMutation {
    EntityID target;
    float amount;
};

struct AddItemMutation {
    EntityID entity_id;
    std::string item;
    uint32_t count = 1;
};

struct RemoveItemMutation {
    EntityID entity_id;
    std::string item;
    uint32_t count = 1;
};

struct LogMessageMutation {
    std::string message;
    std::string level = "info"; // info, warn, error
};

using Mutation = std::variant<
    SetStateMutation,
    SpawnEntityMutation,
    DestroyEntityMutation,
    TriggerDialogueMutation,
    PlaySoundMutation,
    SetTransformMutation,
    DamageEntityMutation,
    HealEntityMutation,
    AddItemMutation,
    RemoveItemMutation,
    LogMessageMutation
>;

class ScriptResult {
public:
    // Fluent API for building mutations
    ScriptResult& set(std::string_view key, std::any value);
    ScriptResult& spawn_entity(std::string_view prefab, vec3 position, std::string_view name = "");
    ScriptResult& destroy_entity(EntityID id);
    ScriptResult& trigger_dialogue(std::string_view id, EntityID speaker = INVALID_ENTITY);
    ScriptResult& play_sound(std::string_view sound, vec3 pos = vec3{0.f}, float volume = 1.0f);
    ScriptResult& set_transform(EntityID id, const Transform& transform);
    ScriptResult& damage(EntityID target, float amount, EntityID source = INVALID_ENTITY);
    ScriptResult& heal(EntityID target, float amount);
    ScriptResult& add_item(EntityID entity, std::string_view item, uint32_t count = 1);
    ScriptResult& remove_item(EntityID entity, std::string_view item, uint32_t count = 1);
    ScriptResult& log(std::string_view message, std::string_view level = "info");

    // Access mutations
    const std::vector<Mutation>& mutations() const { return mutations_; }
    bool empty() const { return mutations_.empty(); }
    size_t mutation_count() const { return mutations_.size(); }

    // Merge another result into this one
    void merge(const ScriptResult& other);

    // Clear all mutations
    void clear();

private:
    std::vector<Mutation> mutations_;
};

} // namespace odyssey::scripting
