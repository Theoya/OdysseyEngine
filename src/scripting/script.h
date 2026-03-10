#pragma once
#include "scripting/script_context.h"
#include "scripting/script_result.h"
#include <string>

namespace odyssey::scripting {

class Script {
public:
    virtual ~Script() = default;

    // Pure: takes world state snapshot, returns mutations
    // Same input must always produce same output
    virtual ScriptResult tick(const ScriptContext& ctx) = 0;

    // Called once when script is first attached to an entity
    virtual void on_attach(EntityID owner) { owner_id_ = owner; }

    // Script metadata
    virtual std::string name() const = 0;

    EntityID owner() const { return owner_id_; }

protected:
    EntityID owner_id_ = INVALID_ENTITY;
};

} // namespace odyssey::scripting
