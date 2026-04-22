#pragma once
#include "core/result.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <optional>
#include <string>
#include <vector>

namespace odyssey::vulkan {

/// ---------------------------------------------------------------------------
/// M4 artifact: VK_EXT_descriptor_indexing feature flags enabled in Phase 6.
///
/// Each flag is required for the bindless texture array.  Device creation fails
/// with DeviceCreateErr::MissingDescriptorIndexingFeatures if any are absent —
/// no dual-path renderer, no silent degradation (architect + coder condition).
///
/// Flags and why:
///   descriptorIndexing            — enables the feature set itself (parent gate)
///   runtimeDescriptorArray        — allows array size declared at GLSL runtime
///                                   (layout(binding=0) uniform sampler2D tex[])
///   descriptorBindingPartiallyBound — slots can be unwritten at draw time so
///                                   long as the shader only reads allocated ones
///   descriptorBindingVariableDescriptorCount — the array's actual count can be
///                                   set at descriptor-set allocation, not fixed
///                                   at layout time; lets us size to the actual
///                                   MAX_BINDLESS_TEXTURES constant at runtime
///   descriptorBindingSampledImageUpdateAfterBind — the sampled-image array can
///                                   be updated while already bound in a command
///                                   buffer (no rebind on texture streaming /
///                                   hot-reload); the per-type Vk12 field
///                                   corresponds to the extension's generic
///                                   descriptorBindingUpdateAfterBind flag
///   shaderSampledImageArrayNonUniformIndexing — permits the nonuniformEXT
///                                   qualifier on dynamic sampler indices so the
///                                   driver issues the SPIR-V NonUniform
///                                   decoration (SPIR-V spec 14.1.1)
/// ---------------------------------------------------------------------------

/// Hard error codes for create_device().
enum class DeviceCreateErr : uint32_t {
    NoPhysicalDevice,                  ///< config.physical_device == VK_NULL_HANDLE
    VkCreateDeviceFailed,              ///< vkCreateDevice returned non-VK_SUCCESS
    VmaCreateAllocatorFailed,          ///< vmaCreateAllocator returned non-VK_SUCCESS
    MissingDescriptorIndexingFeatures, ///< one or more required Vk12 feature bits absent
};

std::string device_create_err_to_string(DeviceCreateErr e);

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
/// Fails with MissingDescriptorIndexingFeatures if any required Vk12 flags absent.
Result<DeviceContext, DeviceCreateErr> create_device(const DeviceConfig& config, VkInstance instance);

/// Impure: destroy the device context (VMA allocator, then logical device).
/// Safe to call on a default-initialized context.
void destroy_device(DeviceContext& ctx);

} // namespace odyssey::vulkan
