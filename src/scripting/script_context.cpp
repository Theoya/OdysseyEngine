#include "scripting/script_context.h"
#include <cmath>
#include <algorithm>

namespace odyssey::scripting {

bool ScriptContext::has_key(std::string_view key) const {
    return state_store_.find(std::string(key)) != state_store_.end();
}

bool ScriptContext::has_item(EntityID entity, std::string_view item) const {
    auto it = entity_items_.find(entity);
    if (it == entity_items_.end()) return false;
    const auto& items = it->second;
    return std::find(items.begin(), items.end(), std::string(item)) != items.end();
}

vec3 ScriptContext::get_position(EntityID entity) const {
    auto it = entity_positions_.find(entity);
    if (it == entity_positions_.end()) return vec3{0.f};
    return it->second;
}

float ScriptContext::get_health(EntityID entity) const {
    auto it = entity_health_.find(entity);
    if (it == entity_health_.end()) return 0.0f;
    return it->second.first;
}

float ScriptContext::get_distance(EntityID a, EntityID b) const {
    vec3 pos_a = get_position(a);
    vec3 pos_b = get_position(b);
    vec3 diff = pos_a - pos_b;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
}

bool ScriptContext::is_alive(EntityID entity) const {
    auto it = entity_health_.find(entity);
    if (it == entity_health_.end()) return false;
    return it->second.first > 0.0f;
}

std::vector<EntityID> ScriptContext::entities_in_radius(vec3 center, float radius) const {
    std::vector<EntityID> result;
    float radius_sq = radius * radius;
    for (const auto& [id, pos] : entity_positions_) {
        vec3 diff = pos - center;
        float dist_sq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (dist_sq <= radius_sq) {
            result.push_back(id);
        }
    }
    return result;
}

EntityID ScriptContext::find_entity(std::string_view name) const {
    auto it = name_to_entity_.find(std::string(name));
    if (it == name_to_entity_.end()) return INVALID_ENTITY;
    return it->second;
}

bool ScriptContext::is_key_pressed(std::string_view key) const {
    auto it = key_states_.find(std::string(key));
    if (it == key_states_.end()) return false;
    return it->second.first;
}

bool ScriptContext::is_key_just_pressed(std::string_view key) const {
    auto it = key_states_.find(std::string(key));
    if (it == key_states_.end()) return false;
    return it->second.second;
}

void ScriptContext::set_state(const std::string& key, std::any value) {
    state_store_[key] = std::move(value);
}

void ScriptContext::add_entity_position(EntityID id, vec3 pos) {
    entity_positions_[id] = pos;
}

void ScriptContext::add_entity_health(EntityID id, float health, float max_health) {
    entity_health_[id] = {health, max_health};
}

void ScriptContext::add_entity_name(EntityID id, const std::string& name) {
    entity_names_[id] = name;
    name_to_entity_[name] = id;
}

void ScriptContext::add_item(EntityID entity, const std::string& item) {
    entity_items_[entity].push_back(item);
}

void ScriptContext::set_key_state(const std::string& key, bool pressed, bool just_pressed) {
    key_states_[key] = {pressed, just_pressed};
}

void ScriptContext::clear() {
    state_store_.clear();
    entity_positions_.clear();
    entity_health_.clear();
    entity_names_.clear();
    name_to_entity_.clear();
    entity_items_.clear();
    key_states_.clear();
    player_id = INVALID_ENTITY;
    delta_time = 0.0f;
    total_time = 0.0f;
    frame_number = 0;
}

} // namespace odyssey::scripting
