#pragma once
#include "core/result.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <utility>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Descriptor binding description
// ---------------------------------------------------------------------------

struct DescriptorBindingDesc {
    uint32_t binding;
    VkDescriptorType type;          ///< Typically VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    VkShaderStageFlags stage;       ///< Typically VK_SHADER_STAGE_COMPUTE_BIT
};

// ---------------------------------------------------------------------------
// Compute pipeline configuration and context
// ---------------------------------------------------------------------------

/// Pure input: everything needed to create a compute pipeline.
struct ComputePipelineConfig {
    std::vector<uint32_t> spirv_code;
    std::vector<DescriptorBindingDesc> bindings;
    uint32_t push_constant_size = 0;        ///< Optional push constant range size.
};

/// The live compute pipeline objects.
struct ComputePipelineContext {
    VkPipeline pipeline                        = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout           = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkShaderModule shader_module               = VK_NULL_HANDLE;
};

/// Impure: create a compute pipeline from SPIR-V bytecode and binding descriptions.
Result<ComputePipelineContext> create_compute_pipeline(
    VkDevice device,
    const ComputePipelineConfig& config
);

/// Impure: destroy all pipeline objects. Safe on default-initialized context.
void destroy_compute_pipeline(VkDevice device, ComputePipelineContext& ctx);

// ---------------------------------------------------------------------------
// Descriptor pool / set management
// ---------------------------------------------------------------------------

/// Pure output: descriptor pool sizing.
struct DescriptorPoolConfig {
    uint32_t max_sets;
    std::vector<VkDescriptorPoolSize> pool_sizes;
};

/// Pure: compute how big the descriptor pool needs to be for the given archetype count.
DescriptorPoolConfig compute_descriptor_pool_config(
    uint32_t archetype_count,
    uint32_t buffers_per_archetype
);

/// Impure: create a descriptor pool.
Result<VkDescriptorPool> create_descriptor_pool(
    VkDevice device,
    const DescriptorPoolConfig& config
);

/// Impure: destroy a descriptor pool. Safe to call with VK_NULL_HANDLE.
void destroy_descriptor_pool(VkDevice device, VkDescriptorPool pool);

/// Impure: allocate a single descriptor set from the pool.
Result<VkDescriptorSet> allocate_descriptor_set(
    VkDevice device,
    VkDescriptorPool pool,
    VkDescriptorSetLayout layout
);

/// Impure: write buffer bindings into a descriptor set.
/// Each pair is (binding index, VkBuffer). Sizes must match 1:1.
void update_descriptor_set(
    VkDevice device,
    VkDescriptorSet set,
    const std::vector<std::pair<uint32_t, VkBuffer>>& buffer_bindings,
    const std::vector<VkDeviceSize>& buffer_sizes
);

} // namespace odyssey::vulkan
