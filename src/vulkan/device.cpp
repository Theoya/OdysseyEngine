#include "vulkan/device.h"

#include <spdlog/spdlog.h>

#include <set>
#include <string>
#include <vector>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Pure: find queue families
// ---------------------------------------------------------------------------

QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());

    for (uint32_t i = 0; i < family_count; ++i) {
        const auto& props = families[i];

        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }

        if (props.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            // Prefer a dedicated compute queue (no graphics bit) if available.
            if (!indices.compute.has_value() || !(props.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.compute = i;
            }
        }

        VkBool32 present_support = VK_FALSE;
        if (surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        }
        if (present_support) {
            indices.present = i;
        }

        if (indices.is_complete()) {
            break;
        }
    }

    return indices;
}

// ---------------------------------------------------------------------------
// Pure: score a physical device
// ---------------------------------------------------------------------------

uint32_t score_physical_device(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(device, &mem_props);

    uint32_t score = 0;

    // Strongly prefer discrete GPUs.
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 10000;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 1000;
    }

    // Bonus for max 2D image dimension (proxy for GPU capability).
    score += props.limits.maxImageDimension2D;

    // Bonus for total device-local memory (in MB).
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
        if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            score += static_cast<uint32_t>(mem_props.memoryHeaps[i].size / (1024 * 1024));
        }
    }

    return score;
}

// ---------------------------------------------------------------------------
// Helper: check device extension support
// ---------------------------------------------------------------------------

static bool check_device_extension_support(VkPhysicalDevice device,
                                           const std::vector<const char*>& required) {
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> available(ext_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, available.data());

    std::set<std::string> remaining(required.begin(), required.end());
    for (const auto& ext : available) {
        remaining.erase(ext.extensionName);
    }
    return remaining.empty();
}

// ---------------------------------------------------------------------------
// Pure: select best physical device
// ---------------------------------------------------------------------------

DeviceConfig select_physical_device(VkInstance instance, VkSurfaceKHR surface,
                                    uint32_t preferred_gpu_index) {
    DeviceConfig config;
    config.required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0) {
        spdlog::error("No Vulkan-capable physical devices found");
        return config;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    spdlog::info("Found {} physical device(s)", device_count);

    // If a preferred index was given and is valid, try that first.
    if (preferred_gpu_index > 0 && preferred_gpu_index <= device_count) {
        VkPhysicalDevice preferred = devices[preferred_gpu_index - 1];
        auto families = find_queue_families(preferred, surface);
        if (families.is_complete() &&
            check_device_extension_support(preferred, config.required_extensions)) {
            config.physical_device = preferred;
            config.queue_families  = families;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(preferred, &props);
            spdlog::info("Using preferred GPU #{}: {}", preferred_gpu_index, props.deviceName);
            return config;
        }
        spdlog::warn("Preferred GPU #{} is not suitable, falling back to scoring", preferred_gpu_index);
    }

    // Score all devices and pick the best.
    uint32_t best_score = 0;
    for (const auto& device : devices) {
        auto families = find_queue_families(device, surface);
        if (!families.is_complete()) continue;
        if (!check_device_extension_support(device, config.required_extensions)) continue;

        uint32_t s = score_physical_device(device);
        if (s > best_score) {
            best_score             = s;
            config.physical_device = device;
            config.queue_families  = families;
        }
    }

    if (config.physical_device != VK_NULL_HANDLE) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(config.physical_device, &props);
        spdlog::info("Selected GPU: {} (score {})", props.deviceName, best_score);
    } else {
        spdlog::error("No suitable physical device found");
    }

    return config;
}

// ---------------------------------------------------------------------------
// Impure: create logical device + VMA allocator
// ---------------------------------------------------------------------------

Result<DeviceContext> create_device(const DeviceConfig& config, VkInstance instance) {
    if (config.physical_device == VK_NULL_HANDLE) {
        return Result<DeviceContext>::err("No physical device selected");
    }

    // Collect unique queue families.
    std::set<uint32_t> unique_families;
    if (config.queue_families.graphics.has_value())
        unique_families.insert(config.queue_families.graphics.value());
    if (config.queue_families.compute.has_value())
        unique_families.insert(config.queue_families.compute.value());
    if (config.queue_families.present.has_value())
        unique_families.insert(config.queue_families.present.value());

    float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_cis;
    for (uint32_t family : unique_families) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &queue_priority;
        queue_cis.push_back(qi);
    }

    VkPhysicalDeviceFeatures device_features{};
    // Enable any features we need (none critical for Phase 1).

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = static_cast<uint32_t>(queue_cis.size());
    ci.pQueueCreateInfos       = queue_cis.data();
    ci.pEnabledFeatures        = &device_features;
    ci.enabledExtensionCount   = static_cast<uint32_t>(config.required_extensions.size());
    ci.ppEnabledExtensionNames = config.required_extensions.data();

    DeviceContext ctx;
    ctx.physical_device = config.physical_device;
    ctx.queue_families  = config.queue_families;

    VkResult result = vkCreateDevice(config.physical_device, &ci, nullptr, &ctx.device);
    if (result != VK_SUCCESS) {
        return Result<DeviceContext>::err(
            "vkCreateDevice failed with code " + std::to_string(static_cast<int>(result)));
    }

    // Retrieve queues.
    vkGetDeviceQueue(ctx.device, config.queue_families.graphics.value(), 0, &ctx.graphics_queue);
    vkGetDeviceQueue(ctx.device, config.queue_families.compute.value(),  0, &ctx.compute_queue);
    vkGetDeviceQueue(ctx.device, config.queue_families.present.value(),  0, &ctx.present_queue);

    // Create VMA allocator.
    VmaAllocatorCreateInfo alloc_ci{};
    alloc_ci.physicalDevice = config.physical_device;
    alloc_ci.device         = ctx.device;
    alloc_ci.instance       = instance;
    alloc_ci.vulkanApiVersion = VK_API_VERSION_1_3;

    result = vmaCreateAllocator(&alloc_ci, &ctx.allocator);
    if (result != VK_SUCCESS) {
        vkDestroyDevice(ctx.device, nullptr);
        ctx.device = VK_NULL_HANDLE;
        return Result<DeviceContext>::err(
            "vmaCreateAllocator failed with code " + std::to_string(static_cast<int>(result)));
    }

    spdlog::info("Logical device and VMA allocator created");
    return Result<DeviceContext>::ok(std::move(ctx));
}

void destroy_device(DeviceContext& ctx) {
    if (ctx.allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(ctx.allocator);
        ctx.allocator = VK_NULL_HANDLE;
        spdlog::info("VMA allocator destroyed");
    }
    if (ctx.device != VK_NULL_HANDLE) {
        vkDestroyDevice(ctx.device, nullptr);
        ctx.device = VK_NULL_HANDLE;
        spdlog::info("Logical device destroyed");
    }
}

} // namespace odyssey::vulkan
