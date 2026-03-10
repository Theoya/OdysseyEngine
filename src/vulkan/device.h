#pragma once
#include "core/result.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <optional>
#include <vector>

namespace odyssey::vulkan {

/// Queue family indices discovered on a physical device.
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> present;

    /// True when all required queue families have been found.
    bool is_complete() const {
        return graphics.has_value() && compute.has_value() && present.has_value();
    }
};

/// Pure output: everything needed to create a logical device.
struct DeviceConfig {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    QueueFamilyIndices queue_families;
    std::vector<const char*> required_extensions;
};

/// Pure: find queue family indices that satisfy graphics, compute, and present.
QueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);

/// Pure: score a physical device (higher is better). Discrete GPUs score highest.
uint32_t score_physical_device(VkPhysicalDevice device);

/// Pure: enumerate all physical devices, score them, and return the best config.
/// If preferred_gpu_index is non-zero and valid, that device is selected instead.
DeviceConfig select_physical_device(VkInstance instance, VkSurfaceKHR surface,
                                    uint32_t preferred_gpu_index = 0);

/// The fully-initialized device context: logical device, queues, and allocator.
struct DeviceContext {
    VkDevice device                  = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkQueue graphics_queue           = VK_NULL_HANDLE;
    VkQueue compute_queue            = VK_NULL_HANDLE;
    VkQueue present_queue            = VK_NULL_HANDLE;
    QueueFamilyIndices queue_families;
    VmaAllocator allocator           = VK_NULL_HANDLE;
};

/// Impure: create logical device, retrieve queues, and create VMA allocator.
Result<DeviceContext> create_device(const DeviceConfig& config, VkInstance instance);

/// Impure: destroy the device context (VMA allocator, then logical device).
/// Safe to call on a default-initialized context.
void destroy_device(DeviceContext& ctx);

} // namespace odyssey::vulkan
