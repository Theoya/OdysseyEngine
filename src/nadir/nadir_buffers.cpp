#include "nadir/nadir_buffers.h"

#include <spdlog/spdlog.h>

namespace odyssey::nadir {

// ---------------------------------------------------------------------------
// Pure: compute sizes for all 7 buffers given an archetype description
// ---------------------------------------------------------------------------
BufferSetLayout compute_buffer_set_layout(const ArchetypeBufferDesc& desc) {
    BufferSetLayout layout;

    const uint64_t n = static_cast<uint64_t>(desc.entity_count);

    layout.transform_size = BufferSetLayout::POSITION_STRIDE * n;
    layout.stats_size     = BufferSetLayout::STATS_STRIDE * n;
    layout.persist_size   = BufferSetLayout::PERSIST_STRIDE * n;
    layout.output_size    = BufferSetLayout::OUTPUT_STRIDE * n;

    layout.world_state_size = BufferSetLayout::WORLD_STATE_SIZE;

    if (desc.needs_spatial_grid) {
        layout.spatial_size =
            BufferSetLayout::SPATIAL_CELL_STRIDE *
            BufferSetLayout::DEFAULT_SPATIAL_CELLS;
    }

    if (desc.needs_debug_output) {
        layout.debug_size = BufferSetLayout::DEBUG_STRIDE * n;
    }

    layout.total_size = layout.transform_size
                      + layout.stats_size
                      + layout.spatial_size
                      + layout.world_state_size
                      + layout.persist_size
                      + layout.output_size
                      + layout.debug_size;

    return layout;
}

// ---------------------------------------------------------------------------
// Helper: create a single VkBuffer via VMA
// ---------------------------------------------------------------------------
static Result<std::pair<VkBuffer, VmaAllocation>> create_single_buffer(
    VmaAllocator allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memory_usage)
{
    if (size == 0) {
        // Return null handles for zero-size buffers (e.g. disabled spatial/debug)
        return Result<std::pair<VkBuffer, VmaAllocation>>::ok({VK_NULL_HANDLE, VK_NULL_HANDLE});
    }

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = memory_usage;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    VkResult vk_result = vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                                         &buffer, &allocation, nullptr);
    if (vk_result != VK_SUCCESS) {
        return Result<std::pair<VkBuffer, VmaAllocation>>::err(
            "vmaCreateBuffer failed with VkResult " + std::to_string(vk_result));
    }

    return Result<std::pair<VkBuffer, VmaAllocation>>::ok({buffer, allocation});
}

// ---------------------------------------------------------------------------
// Impure: create the full set of 7 buffers for an archetype
// ---------------------------------------------------------------------------
Result<BufferSet> create_buffer_set(VmaAllocator allocator,
                                    const BufferSetLayout& layout) {
    BufferSet set;
    set.layout = layout;

    // Buffer 0: Transforms — GPU storage, CPU writes via transfer
    auto transforms = create_single_buffer(
        allocator, layout.transform_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    if (!transforms.is_ok()) return Result<BufferSet>::err(transforms.error());
    set.transforms = transforms.value().first;
    set.transform_alloc = transforms.value().second;

    // Buffer 1: Stats — GPU storage, CPU writes via transfer
    auto stats = create_single_buffer(
        allocator, layout.stats_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    if (!stats.is_ok()) return Result<BufferSet>::err(stats.error());
    set.stats = stats.value().first;
    set.stats_alloc = stats.value().second;

    // Buffer 2: Spatial Grid — GPU storage, CPU writes via transfer
    auto spatial = create_single_buffer(
        allocator, layout.spatial_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    if (!spatial.is_ok()) return Result<BufferSet>::err(spatial.error());
    set.spatial = spatial.value().first;
    set.spatial_alloc = spatial.value().second;

    // Buffer 3: World State — uniform buffer, CPU writes via transfer
    auto world_state = create_single_buffer(
        allocator, layout.world_state_size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    if (!world_state.is_ok()) return Result<BufferSet>::err(world_state.error());
    set.world_state = world_state.value().first;
    set.world_state_alloc = world_state.value().second;

    // Buffer 4: Persistent State — GPU storage, read/write from compute shader
    auto persist = create_single_buffer(
        allocator, layout.persist_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    if (!persist.is_ok()) return Result<BufferSet>::err(persist.error());
    set.persist_state = persist.value().first;
    set.persist_alloc = persist.value().second;

    // Buffer 5: Behavior Output — GPU storage, CPU readback via transfer
    auto output = create_single_buffer(
        allocator, layout.output_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    if (!output.is_ok()) return Result<BufferSet>::err(output.error());
    set.output = output.value().first;
    set.output_alloc = output.value().second;

    // Buffer 6: Debug Output — GPU storage, CPU readback via transfer
    auto debug = create_single_buffer(
        allocator, layout.debug_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    if (!debug.is_ok()) return Result<BufferSet>::err(debug.error());
    set.debug_output = debug.value().first;
    set.debug_alloc = debug.value().second;

    spdlog::info("Created buffer set: {} bytes total ({} entities)",
                 layout.total_size, layout.transform_size / BufferSetLayout::POSITION_STRIDE);

    return Result<BufferSet>::ok(std::move(set));
}

// ---------------------------------------------------------------------------
// Impure: destroy all buffers in the set
// ---------------------------------------------------------------------------
static void destroy_if_valid(VmaAllocator allocator, VkBuffer& buffer, VmaAllocation& alloc) {
    if (buffer != VK_NULL_HANDLE && alloc != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, buffer, alloc);
        buffer = VK_NULL_HANDLE;
        alloc = VK_NULL_HANDLE;
    }
}

void destroy_buffer_set(VmaAllocator allocator, BufferSet& set) {
    destroy_if_valid(allocator, set.transforms, set.transform_alloc);
    destroy_if_valid(allocator, set.stats, set.stats_alloc);
    destroy_if_valid(allocator, set.spatial, set.spatial_alloc);
    destroy_if_valid(allocator, set.world_state, set.world_state_alloc);
    destroy_if_valid(allocator, set.persist_state, set.persist_alloc);
    destroy_if_valid(allocator, set.output, set.output_alloc);
    destroy_if_valid(allocator, set.debug_output, set.debug_alloc);

    set.layout = {};

    spdlog::debug("Destroyed buffer set");
}

} // namespace odyssey::nadir
