#pragma once
#include "scripting/script_runner.h"
#include <string>

namespace odyssey::scripting {

// Register all built-in script classes with a ScriptRunner
void register_builtin_scripts(ScriptRunner& runner);

// Macro for easy script registration in .cpp files
#define REGISTER_SCRIPT(ClassName) \
    namespace { \
        struct ClassName##Registrar { \
            ClassName##Registrar() { \
                odyssey::scripting::detail::register_script_factory( \
                    #ClassName, \
                    []() -> std::unique_ptr<odyssey::scripting::Script> { \
                        return std::make_unique<ClassName>(); \
                    } \
                ); \
            } \
        }; \
        static ClassName##Registrar s_##ClassName##_registrar; \
    }

namespace detail {
    void register_script_factory(const std::string& name, ScriptFactory factory);
    const std::unordered_map<std::string, ScriptFactory>& get_registered_factories();
}

} // namespace odyssey::scripting
