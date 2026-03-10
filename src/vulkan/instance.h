#pragma once
#include "core/result.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace odyssey::vulkan {

/// Pure: configuration describing what extensions and layers the instance needs.
struct InstanceConfig {
    std::string app_name = "OdysseyEngine";
    uint32_t app_version = VK_MAKE_VERSION(0, 1, 0);
    bool enable_validation = true;
    std::vector<const char*> required_extensions;
    std::vector<const char*> required_layers;
};

/// Pure: compute the instance configuration based on whether validation is enabled.
/// Queries GLFW for required surface extensions and adds debug utils if validation is on.
InstanceConfig compute_instance_config(bool validation_enabled);

/// Impure: create a VkInstance from the given configuration.
Result<VkInstance> create_instance(const InstanceConfig& config);

/// Impure: destroy a VkInstance. Safe to call with VK_NULL_HANDLE.
void destroy_instance(VkInstance instance);

/// Impure: create a debug messenger that routes validation messages to spdlog.
Result<VkDebugUtilsMessengerEXT> create_debug_messenger(VkInstance instance);

/// Impure: destroy a debug messenger. Safe to call with VK_NULL_HANDLE.
void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger);

} // namespace odyssey::vulkan
