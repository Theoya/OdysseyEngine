#pragma once
#include "core/types.h"
#include "core/result.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/bindless_texture_registry.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <array>
#include <utility>

namespace odyssey::vulkan {

/// Push constants for the basic shader.
/// Phase 6: extended with material_index for bindless texture lookup.
/// Layout must match basic.vert / basic.frag push_constant block exactly.
/// Byte layout:
///   offset 0:  mvp            (mat4, 64 bytes)
///   offset 64: color          (vec4, 16 bytes)
///   offset 80: material_index (uint32_t, 4 bytes) — lower 16 bits = bindless slot
///   offset 84: _pad0/1/2      (3×uint32_t, 12 bytes — pad to 96 = multiple of 16)
/// Total: 96 bytes.
struct BasicPushConstants {
    mat4     mvp;            // offset 0
    vec4     color;          // offset 64
    uint32_t material_index; // offset 80: 0 = use color, >0 = bindless slot
    uint32_t _pad0 = 0;      // offset 84
    uint32_t _pad1 = 0;      // offset 88
    uint32_t _pad2 = 0;      // offset 92
};
static_assert(sizeof(BasicPushConstants) == 96,
    "BasicPushConstants must be 96 bytes to match shader push_constant block");

// A simple mesh: vertex buffer + index buffer
struct PrimitiveMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_alloc = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation index_alloc = VK_NULL_HANDLE;
    uint32_t index_count = 0;
};

/// Vertex format: position + normal + UV.
/// UV added in Phase 6 for bindless texture sampling.
/// Attribute layout for basic.vert:
///   location 0: inPosition (vec3, offset 0)
///   location 1: inNormal   (vec3, offset 12)
///   location 2: inUV       (vec2, offset 24)
/// Total stride: 32 bytes.
struct BasicVertex {
    vec3 position;
    vec3 normal;
    vec2 uv{0.0f, 0.0f}; // default 0,0 for untextured primitives
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

    void draw(const mat4& mvp, const vec4& color, PrimitiveType mesh_type,
              uint32_t material_index = 0u);
    void end_frame(VkCommandBuffer cmd);

    /// Recreate extent-dependent resources (depth buffer, framebuffers) after swapchain resize.
    Result<bool> recreate_for_resize(VkExtent2D new_extent,
                                      const std::vector<VkImageView>& swapchain_views);

    /// Attach the bindless texture registry so the pipeline layout can include
    /// set=0.  Must be called BEFORE initialize() if textures are needed,
    /// or the pipeline will use push-constant-color only.
    /// The registry is NOT owned by the renderer; caller manages its lifetime.
    void attach_bindless_registry(const BindlessTextureRegistry* registry) {
        bindless_registry_ = registry;
    }

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

    // Bindless texture registry (set=0). Not owned — registry lives in Engine.
    // Null = no bindless textures (pre-registry path, push material_index=0).
    const BindlessTextureRegistry* bindless_registry_ = nullptr;

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
