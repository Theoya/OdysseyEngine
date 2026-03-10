#pragma once
#include "core/types.h"
#include "core/result.h"
#include "vulkan/device.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstdint>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Buffer layout descriptor (pure computation, no GPU calls)
// ---------------------------------------------------------------------------

struct BufferLayout {
    VkDeviceSize size            = 0;
    VkBufferUsageFlags usage     = 0;
    VmaMemoryUsage memory_usage  = VMA_MEMORY_USAGE_AUTO;
    VmaAllocationCreateFlags alloc_flags = 0;
};

/// Pure: layout for per-entity Transform SSBOs.
BufferLayout compute_transform_buffer_layout(uint32_t entity_count);

/// Pure: layout for per-entity EntityStats SSBOs.
BufferLayout compute_stats_buffer_layout(uint32_t entity_count);

/// Pure: layout for per-entity BehaviorOutput SSBOs (needs readback).
BufferLayout compute_behavior_output_layout(uint32_t entity_count);

/// Pure: layout for the global WorldState uniform buffer.
BufferLayout compute_world_state_layout();

/// Pure: layout for per-entity AgentPersistState SSBOs.
BufferLayout compute_persist_state_layout(uint32_t entity_count);

/// Pure: layout for per-entity debug output SSBO (same size as BehaviorOutput).
BufferLayout compute_debug_output_layout(uint32_t entity_count);

// ---------------------------------------------------------------------------
// GPU buffer handle
// ---------------------------------------------------------------------------

struct Buffer {
    VkBuffer buffer          = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size        = 0;
    void* mapped             = nullptr; ///< Non-null when the buffer is host-visible and mapped.
};

// ---------------------------------------------------------------------------
// Impure buffer operations
// ---------------------------------------------------------------------------

/// Create a GPU buffer according to the given layout.
Result<Buffer> create_buffer(VmaAllocator allocator, const BufferLayout& layout);

/// Destroy a buffer. Safe to call on a default-initialized Buffer.
void destroy_buffer(VmaAllocator allocator, Buffer& buffer);

/// Upload host data into a device-local buffer via a staging buffer.
/// Returns true on success.
Result<bool> upload_buffer_data(
    VmaAllocator allocator,
    const DeviceContext& device_ctx,
    VkCommandPool transfer_pool,
    Buffer& dst,
    const void* data,
    VkDeviceSize size
);

/// Read back data from a device-local buffer via a staging buffer.
Result<std::vector<uint8_t>> readback_buffer_data(
    VmaAllocator allocator,
    const DeviceContext& device_ctx,
    VkCommandPool transfer_pool,
    const Buffer& src,
    VkDeviceSize size
);

} // namespace odyssey::vulkan
