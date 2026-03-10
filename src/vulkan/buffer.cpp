#include "vulkan/buffer.h"
#include "vulkan/command.h"

#include <spdlog/spdlog.h>

#include <cstring>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Pure: compute buffer layouts
// ---------------------------------------------------------------------------

BufferLayout compute_transform_buffer_layout(uint32_t entity_count) {
    BufferLayout layout;
    layout.size         = static_cast<VkDeviceSize>(entity_count) * sizeof(Transform);
    layout.usage        = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    layout.alloc_flags  = 0; // device-local preferred
    return layout;
}

BufferLayout compute_stats_buffer_layout(uint32_t entity_count) {
    BufferLayout layout;
    layout.size         = static_cast<VkDeviceSize>(entity_count) * sizeof(EntityStats);
    layout.usage        = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    layout.alloc_flags  = 0;
    return layout;
}

BufferLayout compute_behavior_output_layout(uint32_t entity_count) {
    BufferLayout layout;
    layout.size         = static_cast<VkDeviceSize>(entity_count) * sizeof(BehaviorOutput);
    layout.usage        = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    layout.alloc_flags  = 0;
    return layout;
}

BufferLayout compute_world_state_layout() {
    BufferLayout layout;
    layout.size         = sizeof(WorldState);
    layout.usage        = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    // Host-visible so we can map and update every frame without staging.
    layout.alloc_flags  = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                        | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    return layout;
}

BufferLayout compute_persist_state_layout(uint32_t entity_count) {
    BufferLayout layout;
    layout.size         = static_cast<VkDeviceSize>(entity_count) * sizeof(AgentPersistState);
    layout.usage        = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    layout.alloc_flags  = 0;
    return layout;
}

BufferLayout compute_debug_output_layout(uint32_t entity_count) {
    BufferLayout layout;
    layout.size         = static_cast<VkDeviceSize>(entity_count) * sizeof(BehaviorOutput);
    layout.usage        = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    layout.alloc_flags  = 0;
    return layout;
}

// ---------------------------------------------------------------------------
// Impure: create / destroy buffer
// ---------------------------------------------------------------------------

Result<Buffer> create_buffer(VmaAllocator allocator, const BufferLayout& layout) {
    VkBufferCreateInfo buf_ci{};
    buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_ci.size  = layout.size;
    buf_ci.usage = layout.usage;
    buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_ci{};
    alloc_ci.usage = layout.memory_usage;
    alloc_ci.flags = layout.alloc_flags;

    Buffer buf;
    buf.size = layout.size;

    VmaAllocationInfo alloc_info{};
    VkResult result = vmaCreateBuffer(allocator, &buf_ci, &alloc_ci,
                                      &buf.buffer, &buf.allocation, &alloc_info);
    if (result != VK_SUCCESS) {
        return Result<Buffer>::err(
            "vmaCreateBuffer failed with code " + std::to_string(static_cast<int>(result)));
    }

    // If the allocation was created as mapped, store the pointer.
    if (alloc_info.pMappedData) {
        buf.mapped = alloc_info.pMappedData;
    }

    return Result<Buffer>::ok(std::move(buf));
}

void destroy_buffer(VmaAllocator allocator, Buffer& buffer) {
    if (buffer.buffer != VK_NULL_HANDLE && buffer.allocation != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
        buffer.buffer     = VK_NULL_HANDLE;
        buffer.allocation = VK_NULL_HANDLE;
        buffer.mapped     = nullptr;
        buffer.size       = 0;
    }
}

// ---------------------------------------------------------------------------
// Impure: upload data to a device-local buffer via staging
// ---------------------------------------------------------------------------

Result<bool> upload_buffer_data(
    VmaAllocator allocator,
    const DeviceContext& device_ctx,
    VkCommandPool transfer_pool,
    Buffer& dst,
    const void* data,
    VkDeviceSize size)
{
    if (size == 0) {
        return Result<bool>::ok(true);
    }

    // If the destination is already host-mapped, just memcpy directly.
    if (dst.mapped) {
        std::memcpy(dst.mapped, data, static_cast<size_t>(size));
        return Result<bool>::ok(true);
    }

    // Create a host-visible staging buffer.
    BufferLayout staging_layout;
    staging_layout.size         = size;
    staging_layout.usage        = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    staging_layout.alloc_flags  = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    auto staging_result = create_buffer(allocator, staging_layout);
    if (staging_result.is_err()) {
        return Result<bool>::err("Failed to create staging buffer: " + staging_result.error());
    }
    Buffer staging = staging_result.value();

    // Copy data into the staging buffer.
    if (staging.mapped) {
        std::memcpy(staging.mapped, data, static_cast<size_t>(size));
    } else {
        void* mapped = nullptr;
        VkResult vr = vmaMapMemory(allocator, staging.allocation, &mapped);
        if (vr != VK_SUCCESS) {
            destroy_buffer(allocator, staging);
            return Result<bool>::err("Failed to map staging buffer");
        }
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vmaUnmapMemory(allocator, staging.allocation);
    }

    // Record and submit a copy command.
    VkCommandBuffer cmd = begin_single_time_commands(device_ctx.device, transfer_pool);

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size      = size;
    vkCmdCopyBuffer(cmd, staging.buffer, dst.buffer, 1, &region);

    end_single_time_commands(device_ctx.device, device_ctx.graphics_queue,
                             transfer_pool, cmd);

    destroy_buffer(allocator, staging);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Impure: read back data from a device-local buffer via staging
// ---------------------------------------------------------------------------

Result<std::vector<uint8_t>> readback_buffer_data(
    VmaAllocator allocator,
    const DeviceContext& device_ctx,
    VkCommandPool transfer_pool,
    const Buffer& src,
    VkDeviceSize size)
{
    if (size == 0) {
        return Result<std::vector<uint8_t>>::ok({});
    }

    // Create a host-visible staging buffer for readback.
    BufferLayout staging_layout;
    staging_layout.size         = size;
    staging_layout.usage        = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    staging_layout.memory_usage = VMA_MEMORY_USAGE_AUTO;
    staging_layout.alloc_flags  = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    auto staging_result = create_buffer(allocator, staging_layout);
    if (staging_result.is_err()) {
        return Result<std::vector<uint8_t>>::err(
            "Failed to create readback staging buffer: " + staging_result.error());
    }
    Buffer staging = staging_result.value();

    // Copy from device buffer to staging.
    VkCommandBuffer cmd = begin_single_time_commands(device_ctx.device, transfer_pool);

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size      = size;
    vkCmdCopyBuffer(cmd, src.buffer, staging.buffer, 1, &region);

    end_single_time_commands(device_ctx.device, device_ctx.graphics_queue,
                             transfer_pool, cmd);

    // Read data from the staging buffer.
    std::vector<uint8_t> result_data(static_cast<size_t>(size));

    if (staging.mapped) {
        std::memcpy(result_data.data(), staging.mapped, static_cast<size_t>(size));
    } else {
        void* mapped = nullptr;
        VkResult vr = vmaMapMemory(allocator, staging.allocation, &mapped);
        if (vr != VK_SUCCESS) {
            destroy_buffer(allocator, staging);
            return Result<std::vector<uint8_t>>::err("Failed to map readback staging buffer");
        }
        std::memcpy(result_data.data(), mapped, static_cast<size_t>(size));
        vmaUnmapMemory(allocator, staging.allocation);
    }

    destroy_buffer(allocator, staging);
    return Result<std::vector<uint8_t>>::ok(std::move(result_data));
}

} // namespace odyssey::vulkan
