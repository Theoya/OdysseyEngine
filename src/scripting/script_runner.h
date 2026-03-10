#pragma once
#include "scripting/script.h"
#include "scripting/script_context.h"
#include "scripting/script_result.h"
#include "core/result.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace odyssey::scripting {

// Script factory function type
using ScriptFactory = std::function<std::unique_ptr<Script>()>;

class ScriptRunner {
public:
    // Register a script class factory
    void register_script_class(const std::string& class_name, ScriptFactory factory);

    // Create and attach a script to an entity
    Result<Script*> attach_script(const std::string& class_name, EntityID entity);

    // Detach all scripts from an entity
    void detach_scripts(EntityID entity);

    // Run all scripts for one frame
    // Returns merged ScriptResult with all mutations
    ScriptResult tick_all(const ScriptContext& ctx);

    // Run scripts for a specific entity
    ScriptResult tick_entity(EntityID entity, const ScriptContext& ctx);

    // Apply mutations to the world (called by engine after collecting all results)
    // This is where side effects happen
    // For now, this just logs -- actual application depends on scene/entity systems
    void apply_mutations(const ScriptResult& result);

    // Get all scripts attached to an entity
    std::vector<Script*> get_scripts(EntityID entity) const;

    // Total script count
    size_t script_count() const;

    // Clear everything
    void clear();

private:
    std::unordered_map<std::string, ScriptFactory> factories_;
    // Entity -> list of scripts
    std::unordered_map<EntityID, std::vector<std::unique_ptr<Script>>> entity_scripts_;
};

} // namespace odyssey::scripting
