#include "vulkan/instance.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <cstring>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Pure: compute instance configuration
// ---------------------------------------------------------------------------

InstanceConfig compute_instance_config(bool validation_enabled) {
    InstanceConfig config;
    config.enable_validation = validation_enabled;

    // Query GLFW for the extensions it needs to create a Vulkan surface.
    uint32_t glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    if (glfw_exts) {
        config.required_extensions.assign(glfw_exts, glfw_exts + glfw_ext_count);
    }

    if (validation_enabled) {
        config.required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        config.required_layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    return config;
}

// ---------------------------------------------------------------------------
// Validation callback
// ---------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("[Vulkan Validation] {}", callback_data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        spdlog::warn("[Vulkan Validation] {}", callback_data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        spdlog::info("[Vulkan Validation] {}", callback_data->pMessage);
    } else {
        spdlog::debug("[Vulkan Validation] {}", callback_data->pMessage);
    }
    return VK_FALSE;
}

// ---------------------------------------------------------------------------
// Helper: check that all requested layers are available
// ---------------------------------------------------------------------------

static bool check_layer_support(const std::vector<const char*>& requested_layers) {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available.data());

    for (const char* name : requested_layers) {
        bool found = false;
        for (const auto& layer : available) {
            if (std::strcmp(name, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            spdlog::warn("Requested validation layer '{}' not available", name);
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Impure: create instance
// ---------------------------------------------------------------------------

Result<VkInstance> create_instance(const InstanceConfig& config) {
    VkApplicationInfo app_info{};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = config.app_name.c_str();
    app_info.applicationVersion = config.app_version;
    app_info.pEngineName        = "OdysseyEngine";
    app_info.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info{};
    create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo        = &app_info;
    create_info.enabledExtensionCount   = static_cast<uint32_t>(config.required_extensions.size());
    create_info.ppEnabledExtensionNames = config.required_extensions.data();

    // Validation layers — gracefully skip if not available
    VkDebugUtilsMessengerCreateInfoEXT debug_ci{};
    bool use_validation = config.enable_validation
                          && !config.required_layers.empty()
                          && check_layer_support(config.required_layers);

    if (config.enable_validation && !use_validation) {
        spdlog::warn("Validation layers not available (install Vulkan SDK). Continuing without them.");
        // Remove the debug utils extension we added since we're not using layers
        auto& exts = const_cast<std::vector<const char*>&>(config.required_extensions);
        std::erase_if(exts, [](const char* e) {
            return std::strcmp(e, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
        });
        create_info.enabledExtensionCount = static_cast<uint32_t>(exts.size());
        create_info.ppEnabledExtensionNames = exts.data();
    }

    if (use_validation) {
        create_info.enabledLayerCount   = static_cast<uint32_t>(config.required_layers.size());
        create_info.ppEnabledLayerNames = config.required_layers.data();

        debug_ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_ci.pfnUserCallback = debug_callback;
        create_info.pNext        = &debug_ci;
    } else {
        create_info.enabledLayerCount = 0;
    }

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        return Result<VkInstance>::err(
            "vkCreateInstance failed with code " + std::to_string(static_cast<int>(result)));
    }

    spdlog::info("Vulkan instance created (app: {}, validation: {})",
                 config.app_name, config.enable_validation ? "on" : "off");
    return Result<VkInstance>::ok(instance);
}

void destroy_instance(VkInstance instance) {
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        spdlog::info("Vulkan instance destroyed");
    }
}

// ---------------------------------------------------------------------------
// Impure: debug messenger
// ---------------------------------------------------------------------------

Result<VkDebugUtilsMessengerEXT> create_debug_messenger(VkInstance instance) {
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!func) {
        return Result<VkDebugUtilsMessengerEXT>::err(
            "vkCreateDebugUtilsMessengerEXT not available");
    }

    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType      = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback  = debug_callback;

    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkResult result = func(instance, &ci, nullptr, &messenger);
    if (result != VK_SUCCESS) {
        return Result<VkDebugUtilsMessengerEXT>::err(
            "Failed to create debug messenger, code " + std::to_string(static_cast<int>(result)));
    }

    spdlog::info("Vulkan debug messenger created");
    return Result<VkDebugUtilsMessengerEXT>::ok(messenger);
}

void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    if (messenger == VK_NULL_HANDLE || instance == VK_NULL_HANDLE) {
        return;
    }
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func) {
        func(instance, messenger, nullptr);
        spdlog::info("Vulkan debug messenger destroyed");
    }
}

} // namespace odyssey::vulkan
