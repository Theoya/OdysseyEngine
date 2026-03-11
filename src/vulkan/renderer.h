#pragma once
#include "core/types.h"
#include "core/result.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <array>
#include <utility>

namespace odyssey::vulkan {

// Push constants for the basic shader
struct BasicPushConstants {
    mat4 mvp;        // model-view-projection
    vec4 color;      // RGBA color
};

// A simple mesh: vertex buffer + index buffer
struct PrimitiveMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_alloc = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation index_alloc = VK_NULL_HANDLE;
    uint32_t index_count = 0;
};

// Vertex format: position + normal
struct BasicVertex {
    vec3 position;
    vec3 normal;
};

enum class PrimitiveType { BOX, SPHERE, GROUND_PLANE, CYLINDER };

class Renderer {
public:
    Result<bool> initialize(
        const DeviceContext& device_ctx,
        const SwapchainContext& swapchain_ctx,
        VkCommandPool command_pool
    );
    void shutdown();

    // Call each frame — render directly to swapchain
    Result<VkCommandBuffer> begin_frame(uint32_t image_index, VkCommandBuffer cmd);

    // Render to an external render pass / framebuffer (e.g. PostProcessor offscreen)
    Result<VkCommandBuffer> begin_frame_offscreen(VkRenderPass render_pass,
                                                   VkFramebuffer framebuffer,
                                                   VkExtent2D extent,
                                                   VkCommandBuffer cmd);

    void draw(const mat4& mvp, const vec4& color, PrimitiveType mesh_type);
    void end_frame(VkCommandBuffer cmd);

    /// Recreate extent-dependent resources (depth buffer, framebuffers) after swapchain resize.
    Result<bool> recreate_for_resize(VkExtent2D new_extent,
                                      const std::vector<VkImageView>& swapchain_views);

    VkRenderPass render_pass() const { return render_pass_; }

private:
    // Vulkan objects
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    // Depth buffer
    VkImage depth_image_ = VK_NULL_HANDLE;
    VmaAllocation depth_alloc_ = VK_NULL_HANDLE;
    VkImageView depth_view_ = VK_NULL_HANDLE;

    // Primitive meshes
    PrimitiveMesh box_mesh_;
    PrimitiveMesh sphere_mesh_;
    PrimitiveMesh ground_mesh_;
    PrimitiveMesh cylinder_mesh_;

    VkExtent2D extent_{};

    // Cached state for current render pass
    VkCommandBuffer active_cmd_ = VK_NULL_HANDLE;

    // Internal helpers
    Result<bool> create_render_pass(VkFormat color_format);
    Result<bool> create_depth_resources(VkExtent2D extent);
    Result<bool> create_framebuffers(const SwapchainContext& sc);
    Result<bool> create_pipeline();
    Result<bool> create_primitive_meshes(VkCommandPool cmd_pool, VkQueue queue);

    PrimitiveMesh create_mesh(const std::vector<BasicVertex>& vertices,
                              const std::vector<uint32_t>& indices,
                              VkCommandPool cmd_pool, VkQueue queue);
    void destroy_mesh(PrimitiveMesh& mesh);

    // Mesh generators (pure)
    static std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> generate_box();
    static std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> generate_sphere(int segments = 16);
    static std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> generate_ground_plane(float size = 100.0f);
    static std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> generate_cylinder(int segments = 16);
};

} // namespace odyssey::vulkan
