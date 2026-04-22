#include "scripting/script_registry.h"
#include <spdlog/spdlog.h>

namespace odyssey::scripting {

namespace detail {

static std::unordered_map<std::string, ScriptFactory>& get_factory_map() {
    static std::unordered_map<std::string, ScriptFactory> factories;
    return factories;
}

void register_script_factory(const std::string& name, ScriptFactory factory) {
    get_factory_map()[name] = std::move(factory);
    spdlog::debug("ScriptRegistry: auto-registered script '{}'", name);
}

const std::unordered_map<std::string, ScriptFactory>& get_registered_factories() {
    return get_factory_map();
}

} // namespace detail

void register_builtin_scripts(ScriptRunner& runner) {
    const auto& factories = detail::get_registered_factories();
    for (const auto& [name, factory] : factories) {
        runner.register_script_class(name, factory);
        spdlog::info("ScriptRegistry: registered built-in script '{}'", name);
    }
}

std::vector<std::string> list_registered_script_classes() {
    const auto& factories = detail::get_registered_factories();
    std::vector<std::string> names;
    for (const auto& [name, factory] : factories) {
        (void)factory;  // unused in this context
        names.push_back(name);
    }
    return names;
}

} // namespace odyssey::scripting
