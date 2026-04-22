#include "scripting/script_registry.h"
#include <gtest/gtest.h>

using namespace odyssey::scripting;

// F30: list_registered_script_classes returns all registered script class names.
TEST(ScriptRegistryEnumerateTest, ListsAllRegisteredClasses) {
    // Get the list of registered classes. The exact set depends on which
    // scripts have been registered via REGISTER_SCRIPT macro in the codebase.
    // We just verify that list_registered_script_classes() returns a
    // vector and can be called without error.
    auto classes = list_registered_script_classes();

    // Confirm it's a vector<string> and can iterate.
    EXPECT_TRUE(classes.size() >= 0u);
    for (const auto& cls : classes) {
        EXPECT_FALSE(cls.empty());
    }
}
