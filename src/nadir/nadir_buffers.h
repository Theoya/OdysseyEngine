#pragma once

#include "core/types.h"
#include "core/result.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <vector>

namespace odyssey::nadir {

// Descriptor for what buffers an archetype needs
struct ArchetypeBufferDesc {
    uint32_t entity_count = 0;
    bool needs_spatial_grid = true;
    bool needs_debug_output = true;
};

// Pure: computed buffer layout for an archetype's SSBO set
struct BufferSetLayout {
    uint64_t transform_size = 0;    // sizeof(vec4) * entity_count (positions)
    uint64_t stats_size = 0;        // sizeof(EntityStatsGPU) * entity_count
    uint64_t spatial_size = 0;      // spatial grid size
    uint64_t world_state_size = 0;  // sizeof(WorldState) — single instance
    uint64_t persist_size = 0;      // sizeof(AgentPersistGPU) * entity_count
    uint64_t output_size = 0;       // sizeof(BehaviorOutputGPU) * entity_count
    uint64_t debug_size = 0;        // sizeof(vec4) * entity_count
    uint64_t total_size = 0;        // sum of all

    // GPU struct strides (matching GLSL std430 layout, in bytes)
    static constexpr uint64_t POSITION_STRIDE = 16;     // vec4
    static constexpr uint64_t STATS_STRIDE = 32;        // 8 floats = 32 bytes
    static constexpr uint64_t PERSIST_STRIDE = 64;      // padded to 64 bytes
    static constexpr uint64_t OUTPUT_STRIDE = 64;       // padded to 64 bytes
    static constexpr uint64_t DEBUG_STRIDE = 16;        // vec4
    static constexpr uint64_t SPATIAL_CELL_STRIDE = 16; // uvec4

    // Default spatial grid dimensions
    static constexpr uint32_t DEFAULT_SPATIAL_CELLS = 4096;

    // WorldState UBO size (std140 layout: padded)
    // float world_time       offset 0   (4 bytes)
    // float delta_time       offset 4   (4 bytes)
    // -- 8 bytes padding to align vec4 --
    // vec4  player_position  offset 16  (16 bytes)
    // uint  frame_number     offset 32  (4 bytes)
    // uint  total_entities   offset 36  (4 bytes)
    // total: 48 bytes, rounded up to 48 (multiple of 16 for std140)
    static constexpr uint64_t WORLD_STATE_SIZE = 48;
};

// Pure: compute buffer layout from archetype description
BufferSetLayout compute_buffer_set_layout(const ArchetypeBufferDesc& desc);

// Managed buffer set for an archetype (7 SSBOs)
struct BufferSet {
    VkBuffer transforms = VK_NULL_HANDLE;
    VkBuffer stats = VK_NULL_HANDLE;
    VkBuffer spatial = VK_NULL_HANDLE;
    VkBuffer world_state = VK_NULL_HANDLE;
    VkBuffer persist_state = VK_NULL_HANDLE;
    VkBuffer output = VK_NULL_HANDLE;
    VkBuffer debug_output = VK_NULL_HANDLE;

    VmaAllocation transform_alloc = VK_NULL_HANDLE;
    VmaAllocation stats_alloc = VK_NULL_HANDLE;
    VmaAllocation spatial_alloc = VK_NULL_HANDLE;
    VmaAllocation world_state_alloc = VK_NULL_HANDLE;
    VmaAllocation persist_alloc = VK_NULL_HANDLE;
    VmaAllocation output_alloc = VK_NULL_HANDLE;
    VmaAllocation debug_alloc = VK_NULL_HANDLE;

    // Host-mapped pointer for world_state UBO (non-null when host-visible)
    void* world_state_mapped = nullptr;

    BufferSetLayout layout;
};

// Impure: create the full buffer set on the GPU via VMA
Result<BufferSet> create_buffer_set(VmaAllocator allocator, const BufferSetLayout& layout);

// Impure: destroy all buffers in the set
void destroy_buffer_set(VmaAllocator allocator, BufferSet& set);

} // namespace odyssey::nadir
