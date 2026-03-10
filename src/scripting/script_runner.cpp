#include "scripting/script_runner.h"
#include <spdlog/spdlog.h>

namespace odyssey::scripting {

void ScriptRunner::register_script_class(const std::string& class_name, ScriptFactory factory) {
    factories_[class_name] = std::move(factory);
    spdlog::debug("ScriptRunner: registered script class '{}'", class_name);
}

Result<Script*> ScriptRunner::attach_script(const std::string& class_name, EntityID entity) {
    auto it = factories_.find(class_name);
    if (it == factories_.end()) {
        return Result<Script*>::err("Unknown script class: " + class_name);
    }

    auto script = it->second();
    if (!script) {
        return Result<Script*>::err("Factory returned null for: " + class_name);
    }

    script->on_attach(entity);
    Script* raw_ptr = script.get();
    entity_scripts_[entity].push_back(std::move(script));

    spdlog::debug("ScriptRunner: attached '{}' to entity {}", class_name, entity);
    return Result<Script*>::ok(raw_ptr);
}

void ScriptRunner::detach_scripts(EntityID entity) {
    auto it = entity_scripts_.find(entity);
    if (it != entity_scripts_.end()) {
        spdlog::debug("ScriptRunner: detaching {} scripts from entity {}",
                      it->second.size(), entity);
        entity_scripts_.erase(it);
    }
}

ScriptResult ScriptRunner::tick_all(const ScriptContext& ctx) {
    ScriptResult merged;
    for (auto& [entity_id, scripts] : entity_scripts_) {
        for (auto& script : scripts) {
            ScriptResult result = script->tick(ctx);
            merged.merge(result);
        }
    }
    return merged;
}

ScriptResult ScriptRunner::tick_entity(EntityID entity, const ScriptContext& ctx) {
    ScriptResult merged;
    auto it = entity_scripts_.find(entity);
    if (it == entity_scripts_.end()) return merged;

    for (auto& script : it->second) {
        ScriptResult result = script->tick(ctx);
        merged.merge(result);
    }
    return merged;
}

void ScriptRunner::apply_mutations(const ScriptResult& result) {
    for (const auto& mutation : result.mutations()) {
        std::visit([](const auto& m) {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, SetStateMutation>) {
                spdlog::info("Mutation: set state '{}'", m.key);
            }
            else if constexpr (std::is_same_v<T, SpawnEntityMutation>) {
                spdlog::info("Mutation: spawn '{}' at ({:.1f}, {:.1f}, {:.1f})",
                             m.prefab, m.position.x, m.position.y, m.position.z);
            }
            else if constexpr (std::is_same_v<T, DestroyEntityMutation>) {
                spdlog::info("Mutation: destroy entity {}", m.entity_id);
            }
            else if constexpr (std::is_same_v<T, TriggerDialogueMutation>) {
                spdlog::info("Mutation: trigger dialogue '{}'", m.dialogue_id);
            }
            else if constexpr (std::is_same_v<T, PlaySoundMutation>) {
                spdlog::info("Mutation: play sound '{}' vol={:.2f}", m.sound_id, m.volume);
            }
            else if constexpr (std::is_same_v<T, SetTransformMutation>) {
                spdlog::info("Mutation: set transform for entity {}", m.entity_id);
            }
            else if constexpr (std::is_same_v<T, DamageEntityMutation>) {
                spdlog::info("Mutation: damage entity {} for {:.1f}", m.target, m.amount);
            }
            else if constexpr (std::is_same_v<T, HealEntityMutation>) {
                spdlog::info("Mutation: heal entity {} for {:.1f}", m.target, m.amount);
            }
            else if constexpr (std::is_same_v<T, AddItemMutation>) {
                spdlog::info("Mutation: add {}x '{}' to entity {}",
                             m.count, m.item, m.entity_id);
            }
            else if constexpr (std::is_same_v<T, RemoveItemMutation>) {
                spdlog::info("Mutation: remove {}x '{}' from entity {}",
                             m.count, m.item, m.entity_id);
            }
            else if constexpr (std::is_same_v<T, LogMessageMutation>) {
                if (m.level == "warn") {
                    spdlog::warn("Script: {}", m.message);
                } else if (m.level == "error") {
                    spdlog::error("Script: {}", m.message);
                } else {
                    spdlog::info("Script: {}", m.message);
                }
            }
        }, mutation);
    }
}

std::vector<Script*> ScriptRunner::get_scripts(EntityID entity) const {
    std::vector<Script*> result;
    auto it = entity_scripts_.find(entity);
    if (it != entity_scripts_.end()) {
        for (const auto& script : it->second) {
            result.push_back(script.get());
        }
    }
    return result;
}

size_t ScriptRunner::script_count() const {
    size_t count = 0;
    for (const auto& [entity, scripts] : entity_scripts_) {
        count += scripts.size();
    }
    return count;
}

void ScriptRunner::clear() {
    entity_scripts_.clear();
    factories_.clear();
    spdlog::debug("ScriptRunner: cleared all scripts and factories");
}

} // namespace odyssey::scripting
