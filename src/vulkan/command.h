#pragma once
#include "core/result.h"
#include "vulkan/device.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Command pool management
// ---------------------------------------------------------------------------

/// Impure: create a command pool for the given queue family.
Result<VkCommandPool> create_command_pool(
    VkDevice device,
    uint32_t queue_family_index,
    VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
);

/// Impure: destroy a command pool. Safe with VK_NULL_HANDLE.
void destroy_command_pool(VkDevice device, VkCommandPool pool);

// ---------------------------------------------------------------------------
// Command buffer allocation
// ---------------------------------------------------------------------------

/// Impure: allocate one or more command buffers from a pool.
Result<std::vector<VkCommandBuffer>> allocate_command_buffers(
    VkDevice device,
    VkCommandPool pool,
    uint32_t count,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
);

// ---------------------------------------------------------------------------
// Frame synchronization
// ---------------------------------------------------------------------------

struct FrameSync {
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight           = VK_NULL_HANDLE;
};

/// Impure: create semaphores and fence for one frame.
Result<FrameSync> create_frame_sync(VkDevice device);

/// Impure: destroy frame sync objects. Safe on default-initialized FrameSync.
void destroy_frame_sync(VkDevice device, FrameSync& sync);

// ---------------------------------------------------------------------------
// Single-time command helpers (for staging transfers)
// ---------------------------------------------------------------------------

/// Begin a one-shot command buffer (allocate + begin).
VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool pool);

/// End, submit, wait, and free a one-shot command buffer.
void end_single_time_commands(
    VkDevice device,
    VkQueue queue,
    VkCommandPool pool,
    VkCommandBuffer cmd
);

// ---------------------------------------------------------------------------
// Compute dispatch
// ---------------------------------------------------------------------------

/// Pure output: workgroup counts for vkCmdDispatch.
struct DispatchConfig {
    uint32_t group_count_x = 1;
    uint32_t group_count_y = 1;
    uint32_t group_count_z = 1;
};

/// Pure: compute the number of workgroups needed for a 1D dispatch.
DispatchConfig compute_dispatch_config(uint32_t entity_count, uint32_t workgroup_size);

/// Record a compute dispatch into a command buffer.
void record_compute_dispatch(
    VkCommandBuffer cmd,
    VkPipeline pipeline,
    VkPipelineLayout layout,
    VkDescriptorSet descriptor_set,
    const DispatchConfig& dispatch
);

/// Insert a full memory barrier between compute dispatches
/// (SHADER_WRITE -> SHADER_READ).
void record_compute_barrier(VkCommandBuffer cmd);

/// Maximum number of frames that can be in flight simultaneously.
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

} // namespace odyssey::vulkan
