#pragma once
#include "core/types.h"
#include "core/result.h"
#include "vulkan/device.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <shaderc/shaderc.h>
#include <string>
#include <filesystem>
#include <vector>

namespace odyssey::vulkan {

struct CRTParams {
    float time = 0.0f;
    float curvature = 2.0f;
    float scanline_weight = 0.3f;
    float scanline_count = 480.0f;
    float vignette_strength = 0.8f;
    float chromatic_aberration = 1.0f;
    float brightness = 1.2f;
    float flicker_amount = 0.2f;
};

struct EvaHUDParams {
    float time = 0.0f;
    float alert_level = 0.0f;
    float sync_ratio = 0.85f;
    float health_pct = 1.0f;
    float scan_speed = 1.0f;
    float border_width = 0.002f;
    float opacity = 0.6f;
    float _pad = 0.0f;
};

class PostProcessor {
public:
    Result<bool> initialize(
        const DeviceContext& device_ctx,
        VkExtent2D extent,
        VkFormat color_format,
        VkCommandPool command_pool,
        const std::filesystem::path& shader_dir
    );
    void shutdown();

    /// The offscreen render pass -- scene renders INTO this.
    VkRenderPass scene_render_pass() const { return scene_render_pass_; }
    VkFramebuffer scene_framebuffer() const { return scene_framebuffer_; }

    /// Apply post-processing: reads from offscreen texture, writes to swapchain image.
    void apply(VkCommandBuffer cmd, uint32_t swapchain_image_index,
               const CRTParams& crt, const EvaHUDParams& eva);

    /// Recreate all extent-dependent resources after swapchain resize.
    Result<bool> recreate_for_resize(VkExtent2D new_extent,
                                      const std::vector<VkImageView>& new_swapchain_views);

    /// Must be called after swapchain recreation to rebuild post framebuffers.
    Result<bool> set_swapchain_views(const std::vector<VkImageView>& views, VkExtent2D extent);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};
    VkFormat color_format_ = VK_FORMAT_UNDEFINED;

    // Offscreen render target (scene renders here)
    VkImage offscreen_image_ = VK_NULL_HANDLE;
    VmaAllocation offscreen_alloc_ = VK_NULL_HANDLE;
    VkImageView offscreen_view_ = VK_NULL_HANDLE;
    VkSampler offscreen_sampler_ = VK_NULL_HANDLE;

    // Depth for offscreen
    VkImage offscreen_depth_ = VK_NULL_HANDLE;
    VmaAllocation offscreen_depth_alloc_ = VK_NULL_HANDLE;
    VkImageView offscreen_depth_view_ = VK_NULL_HANDLE;

    // Scene render pass (renders to offscreen)
    VkRenderPass scene_render_pass_ = VK_NULL_HANDLE;
    VkFramebuffer scene_framebuffer_ = VK_NULL_HANDLE;

    // Post-process render pass (renders to swapchain)
    VkRenderPass post_render_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> post_framebuffers_;  // one per swapchain image

    // CRT pipeline
    VkPipelineLayout crt_layout_ = VK_NULL_HANDLE;
    VkPipeline crt_pipeline_ = VK_NULL_HANDLE;

    // EVA HUD pipeline
    VkPipelineLayout eva_layout_ = VK_NULL_HANDLE;
    VkPipeline eva_pipeline_ = VK_NULL_HANDLE;

    // Descriptor for the offscreen texture
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

    // Swapchain image views (borrowed, not owned)
    std::vector<VkImageView> swapchain_views_;

    // Helpers
    Result<bool> create_offscreen_target(VkExtent2D extent, VkFormat format);
    Result<bool> create_render_passes(VkFormat color_format);
    Result<bool> create_framebuffers();
    Result<bool> create_descriptor();
    Result<bool> create_pipelines(const std::filesystem::path& shader_dir);

    VkShaderModule compile_shader(const std::filesystem::path& path, shaderc_shader_kind kind);
};

} // namespace odyssey::vulkan
