#include "scripting/script_result.h"

namespace odyssey::scripting {

ScriptResult& ScriptResult::set(std::string_view key, std::any value) {
    mutations_.push_back(SetStateMutation{std::string(key), std::move(value)});
    return *this;
}

ScriptResult& ScriptResult::spawn_entity(std::string_view prefab, vec3 position, std::string_view name) {
    mutations_.push_back(SpawnEntityMutation{std::string(prefab), position, std::string(name)});
    return *this;
}

ScriptResult& ScriptResult::destroy_entity(EntityID id) {
    mutations_.push_back(DestroyEntityMutation{id});
    return *this;
}

ScriptResult& ScriptResult::trigger_dialogue(std::string_view id, EntityID speaker) {
    mutations_.push_back(TriggerDialogueMutation{std::string(id), speaker});
    return *this;
}

ScriptResult& ScriptResult::play_sound(std::string_view sound, vec3 pos, float volume) {
    mutations_.push_back(PlaySoundMutation{std::string(sound), pos, volume});
    return *this;
}

ScriptResult& ScriptResult::set_transform(EntityID id, const Transform& transform) {
    mutations_.push_back(SetTransformMutation{id, transform});
    return *this;
}

ScriptResult& ScriptResult::damage(EntityID target, float amount, EntityID source) {
    mutations_.push_back(DamageEntityMutation{target, amount, source});
    return *this;
}

ScriptResult& ScriptResult::heal(EntityID target, float amount) {
    mutations_.push_back(HealEntityMutation{target, amount});
    return *this;
}

ScriptResult& ScriptResult::add_item(EntityID entity, std::string_view item, uint32_t count) {
    mutations_.push_back(AddItemMutation{entity, std::string(item), count});
    return *this;
}

ScriptResult& ScriptResult::remove_item(EntityID entity, std::string_view item, uint32_t count) {
    mutations_.push_back(RemoveItemMutation{entity, std::string(item), count});
    return *this;
}

ScriptResult& ScriptResult::log(std::string_view message, std::string_view level) {
    mutations_.push_back(LogMessageMutation{std::string(message), std::string(level)});
    return *this;
}

void ScriptResult::merge(const ScriptResult& other) {
    mutations_.insert(mutations_.end(), other.mutations_.begin(), other.mutations_.end());
}

void ScriptResult::clear() {
    mutations_.clear();
}

} // namespace odyssey::scripting
