#include "vulkan/command.h"

#include <spdlog/spdlog.h>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Impure: command pool
// ---------------------------------------------------------------------------

Result<VkCommandPool> create_command_pool(
    VkDevice device,
    uint32_t queue_family_index,
    VkCommandPoolCreateFlags flags)
{
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = queue_family_index;
    ci.flags            = flags;

    VkCommandPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateCommandPool(device, &ci, nullptr, &pool);
    if (result != VK_SUCCESS) {
        return Result<VkCommandPool>::err(
            "vkCreateCommandPool failed with code " + std::to_string(static_cast<int>(result)));
    }

    spdlog::info("Command pool created for queue family {}", queue_family_index);
    return Result<VkCommandPool>::ok(pool);
}

void destroy_command_pool(VkDevice device, VkCommandPool pool) {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, pool, nullptr);
        spdlog::info("Command pool destroyed");
    }
}

// ---------------------------------------------------------------------------
// Impure: command buffer allocation
// ---------------------------------------------------------------------------

Result<std::vector<VkCommandBuffer>> allocate_command_buffers(
    VkDevice device,
    VkCommandPool pool,
    uint32_t count,
    VkCommandBufferLevel level)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool        = pool;
    alloc_info.level              = level;
    alloc_info.commandBufferCount = count;

    std::vector<VkCommandBuffer> buffers(count);
    VkResult result = vkAllocateCommandBuffers(device, &alloc_info, buffers.data());
    if (result != VK_SUCCESS) {
        return Result<std::vector<VkCommandBuffer>>::err(
            "vkAllocateCommandBuffers failed with code " + std::to_string(static_cast<int>(result)));
    }

    return Result<std::vector<VkCommandBuffer>>::ok(std::move(buffers));
}

// ---------------------------------------------------------------------------
// Impure: frame synchronization
// ---------------------------------------------------------------------------

Result<FrameSync> create_frame_sync(VkDevice device) {
    FrameSync sync;

    VkSemaphoreCreateInfo sem_ci{};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_ci{};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start signaled so first wait succeeds

    VkResult result = vkCreateSemaphore(device, &sem_ci, nullptr, &sync.image_available);
    if (result != VK_SUCCESS) {
        return Result<FrameSync>::err(
            "Failed to create image_available semaphore, code " + std::to_string(static_cast<int>(result)));
    }

    result = vkCreateSemaphore(device, &sem_ci, nullptr, &sync.render_finished);
    if (result != VK_SUCCESS) {
        vkDestroySemaphore(device, sync.image_available, nullptr);
        return Result<FrameSync>::err(
            "Failed to create render_finished semaphore, code " + std::to_string(static_cast<int>(result)));
    }

    result = vkCreateFence(device, &fence_ci, nullptr, &sync.in_flight);
    if (result != VK_SUCCESS) {
        vkDestroySemaphore(device, sync.render_finished, nullptr);
        vkDestroySemaphore(device, sync.image_available, nullptr);
        return Result<FrameSync>::err(
            "Failed to create in_flight fence, code " + std::to_string(static_cast<int>(result)));
    }

    return Result<FrameSync>::ok(std::move(sync));
}

void destroy_frame_sync(VkDevice device, FrameSync& sync) {
    if (sync.in_flight != VK_NULL_HANDLE) {
        vkDestroyFence(device, sync.in_flight, nullptr);
        sync.in_flight = VK_NULL_HANDLE;
    }
    if (sync.render_finished != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, sync.render_finished, nullptr);
        sync.render_finished = VK_NULL_HANDLE;
    }
    if (sync.image_available != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, sync.image_available, nullptr);
        sync.image_available = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// Single-time command helpers
// ---------------------------------------------------------------------------

VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool        = pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &begin_info);
    return cmd;
}

void end_single_time_commands(
    VkDevice device,
    VkQueue queue,
    VkCommandPool pool,
    VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

// ---------------------------------------------------------------------------
// Pure: compute dispatch config
// ---------------------------------------------------------------------------

DispatchConfig compute_dispatch_config(uint32_t entity_count, uint32_t workgroup_size) {
    DispatchConfig config;
    config.group_count_x = (entity_count + workgroup_size - 1) / workgroup_size;
    config.group_count_y = 1;
    config.group_count_z = 1;
    return config;
}

// ---------------------------------------------------------------------------
// Impure: record compute dispatch
// ---------------------------------------------------------------------------

void record_compute_dispatch(
    VkCommandBuffer cmd,
    VkPipeline pipeline,
    VkPipelineLayout layout,
    VkDescriptorSet descriptor_set,
    const DispatchConfig& dispatch)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdDispatch(cmd, dispatch.group_count_x, dispatch.group_count_y, dispatch.group_count_z);
}

void record_compute_barrier(VkCommandBuffer cmd) {
    VkMemoryBarrier barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // src stage
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dst stage
        0,                                      // flags
        1, &barrier,                            // memory barriers
        0, nullptr,                             // buffer memory barriers
        0, nullptr                              // image memory barriers
    );
}

} // namespace odyssey::vulkan
