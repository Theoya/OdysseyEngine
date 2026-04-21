#pragma once

// ---------------------------------------------------------------------------
// scene_viewport_renderer.h
// Phase 2: a self-contained Vulkan scene renderer used by the editor
// viewport to produce an offscreen image that ImGui samples via
// ImGui_ImplVulkan_AddTexture.
//
// Scope is deliberately small — this is not a re-implementation of the
// engine's renderer; it draws a single cube mesh instanced per entity as
// a liminal wireframe. Enough to validate "the scene is live" without
// re-plumbing the engine's frame graph (which remains a Phase 3 goal
// per the Phase 1 decision record).
//
// Barrier contract (/barrier-audit-compatible):
//   Per frame:
//     1. scene render pass begins — initialLayout=UNDEFINED (LOAD_OP_CLEAR).
//     2. scene render pass ends   — finalLayout=SHADER_READ_ONLY_OPTIMAL.
//        The render pass carries a subpass dependency from COLOR_ATTACHMENT
//        _OUTPUT_BIT → FRAGMENT_SHADER_BIT so ImGui's sampler read is
//        correctly ordered against the render-pass writes.
//     3. ImGui samples the offscreen view (bound as combined image sampler).
//     4. Next frame begins — no explicit READ→WRITE barrier needed because
//        step 1 does an UNDEFINED transition with LOAD_OP_CLEAR.
// ---------------------------------------------------------------------------

#include "core/result.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace odyssey::scene { class EntityManager; }
namespace odyssey { class Camera; }

namespace odyssey::editor {

// Per-entity GPU-friendly draw record — mirrors engine's RenderEntity shape
// but local to the editor so there is no engine/editor coupling on a type
// the engine may evolve.
struct SceneDrawEntity {
    float position[3];
    float color[4];
    float scale[3];
};

struct SceneViewportInit {
    VkPhysicalDevice    phys_device    = VK_NULL_HANDLE;
    VkDevice            device         = VK_NULL_HANDLE;
    VkQueue             graphics_queue = VK_NULL_HANDLE;
    uint32_t            graphics_family = 0;
    VkExtent2D          initial_extent{1280, 720};
    VkFormat            color_format   = VK_FORMAT_B8G8R8A8_UNORM;
};

class SceneViewportRenderer {
public:
    SceneViewportRenderer() = default;
    ~SceneViewportRenderer();

    SceneViewportRenderer(const SceneViewportRenderer&) = delete;
    SceneViewportRenderer& operator=(const SceneViewportRenderer&) = delete;

    Result<bool> initialize(const SceneViewportInit& init);
    void shutdown();

    // Resize the offscreen target. Blocks on the device — callers should
    // only invoke this from the main thread between frames, typically in
    // response to an ImGui panel resize.
    Result<bool> resize(VkExtent2D new_extent);

    // Record a scene render pass into `cmd` that draws the entities as
    // colored boxes. `vp` is row-major float[16] view-projection matrix.
    // `cmd` must be in the "recording" state.
    void record(VkCommandBuffer cmd,
                const float vp[16],
                const std::vector<SceneDrawEntity>& entities);

    // Accessors used by viewport_panel.cpp to bind the image into ImGui.
    VkImageView offscreen_view()    const { return offscreen_view_; }
    VkSampler   offscreen_sampler() const { return offscreen_sampler_; }
    VkExtent2D  extent()            const { return extent_; }

    // Diagnostic: returns the offscreen image layout the render pass leaves
    // it in (constant — SHADER_READ_ONLY_OPTIMAL). Exposed for a unit test
    // that asserts the barrier contract at compile time.
    static constexpr VkImageLayout final_layout() {
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

private:
    VkPhysicalDevice phys_device_    = VK_NULL_HANDLE;
    VkDevice         device_         = VK_NULL_HANDLE;
    VkQueue          graphics_queue_ = VK_NULL_HANDLE;
    uint32_t         graphics_family_ = 0;
    VkExtent2D       extent_{0, 0};
    VkFormat         color_format_   = VK_FORMAT_B8G8R8A8_UNORM;

    VkImage          color_image_     = VK_NULL_HANDLE;
    VkDeviceMemory   color_memory_    = VK_NULL_HANDLE;
    VkImageView      offscreen_view_  = VK_NULL_HANDLE;
    VkSampler        offscreen_sampler_ = VK_NULL_HANDLE;

    VkImage          depth_image_     = VK_NULL_HANDLE;
    VkDeviceMemory   depth_memory_    = VK_NULL_HANDLE;
    VkImageView      depth_view_      = VK_NULL_HANDLE;

    VkRenderPass     render_pass_     = VK_NULL_HANDLE;
    VkFramebuffer    framebuffer_     = VK_NULL_HANDLE;

    VkShaderModule   vert_module_     = VK_NULL_HANDLE;
    VkShaderModule   frag_module_     = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline       pipeline_        = VK_NULL_HANDLE;

    VkBuffer         vertex_buffer_   = VK_NULL_HANDLE;
    VkDeviceMemory   vertex_memory_   = VK_NULL_HANDLE;
    VkBuffer         index_buffer_    = VK_NULL_HANDLE;
    VkDeviceMemory   index_memory_    = VK_NULL_HANDLE;
    uint32_t         index_count_     = 0;

    Result<bool> create_images(VkExtent2D extent);
    void destroy_images();

    Result<bool> create_render_pass();
    Result<bool> create_framebuffer();
    Result<bool> create_pipeline();
    Result<bool> create_cube_mesh();

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props) const;
};

} // namespace odyssey::editor
