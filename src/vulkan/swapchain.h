#pragma once
#include "core/result.h"
#include "vulkan/device.h"
#include <vulkan/vulkan.h>
#include <vector>

struct GLFWwindow;

namespace odyssey::vulkan {

/// Pure output: the chosen swapchain parameters.
struct SwapchainConfig {
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    uint32_t image_count;
    VkSurfaceTransformFlagBitsKHR transform;
};

/// Pure: choose the best swapchain config given surface capabilities and window size.
/// Prefers B8G8R8A8_SRGB / SRGB_NONLINEAR, MAILBOX present mode (or FIFO for vsync).
SwapchainConfig compute_swapchain_config(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    uint32_t window_width,
    uint32_t window_height,
    bool vsync
);

/// The live swapchain: handle, images, views, and metadata.
struct SwapchainContext {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
};

/// Impure: create swapchain, retrieve images, and create image views.
/// Pass a previous swapchain handle for seamless recreation on resize.
Result<SwapchainContext> create_swapchain(
    const DeviceContext& device_ctx,
    VkSurfaceKHR surface,
    const SwapchainConfig& config,
    VkSwapchainKHR old_swapchain = VK_NULL_HANDLE
);

/// Impure: destroy image views and swapchain. Safe on default-initialized context.
void destroy_swapchain(VkDevice device, SwapchainContext& ctx);

} // namespace odyssey::vulkan
