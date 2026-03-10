#include "vulkan/swapchain.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <limits>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Pure: compute swapchain configuration
// ---------------------------------------------------------------------------

SwapchainConfig compute_swapchain_config(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    uint32_t window_width,
    uint32_t window_height,
    bool vsync)
{
    // Query surface capabilities.
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &caps);

    // Query surface formats.
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, formats.data());

    // Query present modes.
    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &mode_count, modes.data());

    SwapchainConfig config{};

    // --- Surface format: prefer B8G8R8A8_SRGB with SRGB_NONLINEAR colorspace.
    config.surface_format = formats[0]; // fallback
    for (const auto& fmt : formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            config.surface_format = fmt;
            break;
        }
    }

    // --- Present mode: MAILBOX for low-latency, FIFO for vsync.
    config.present_mode = VK_PRESENT_MODE_FIFO_KHR; // always available
    if (!vsync) {
        for (const auto& mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                config.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    // --- Extent: clamp window dimensions to surface capabilities.
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        config.extent = caps.currentExtent;
    } else {
        config.extent.width  = std::clamp(window_width,
                                          caps.minImageExtent.width,
                                          caps.maxImageExtent.width);
        config.extent.height = std::clamp(window_height,
                                          caps.minImageExtent.height,
                                          caps.maxImageExtent.height);
    }

    // --- Image count: one more than minimum, capped at maximum.
    config.image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && config.image_count > caps.maxImageCount) {
        config.image_count = caps.maxImageCount;
    }

    config.transform = caps.currentTransform;

    return config;
}

// ---------------------------------------------------------------------------
// Impure: create swapchain
// ---------------------------------------------------------------------------

Result<SwapchainContext> create_swapchain(
    const DeviceContext& device_ctx,
    VkSurfaceKHR surface,
    const SwapchainConfig& config,
    VkSwapchainKHR old_swapchain)
{
    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surface;
    ci.minImageCount    = config.image_count;
    ci.imageFormat      = config.surface_format.format;
    ci.imageColorSpace  = config.surface_format.colorSpace;
    ci.imageExtent      = config.extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.preTransform     = config.transform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = config.present_mode;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = old_swapchain;

    // Queue family sharing mode.
    uint32_t graphics_family = device_ctx.queue_families.graphics.value();
    uint32_t present_family  = device_ctx.queue_families.present.value();

    uint32_t family_indices[] = {graphics_family, present_family};
    if (graphics_family != present_family) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = family_indices;
    } else {
        ci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        ci.queueFamilyIndexCount = 0;
        ci.pQueueFamilyIndices   = nullptr;
    }

    SwapchainContext ctx;
    VkResult result = vkCreateSwapchainKHR(device_ctx.device, &ci, nullptr, &ctx.swapchain);
    if (result != VK_SUCCESS) {
        return Result<SwapchainContext>::err(
            "vkCreateSwapchainKHR failed with code " + std::to_string(static_cast<int>(result)));
    }

    ctx.format = config.surface_format.format;
    ctx.extent = config.extent;

    // Retrieve swapchain images.
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(device_ctx.device, ctx.swapchain, &image_count, nullptr);
    ctx.images.resize(image_count);
    vkGetSwapchainImagesKHR(device_ctx.device, ctx.swapchain, &image_count, ctx.images.data());

    // Create image views.
    ctx.image_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo view_ci{};
        view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image    = ctx.images[i];
        view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format   = ctx.format;

        view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.baseMipLevel   = 0;
        view_ci.subresourceRange.levelCount     = 1;
        view_ci.subresourceRange.baseArrayLayer = 0;
        view_ci.subresourceRange.layerCount     = 1;

        result = vkCreateImageView(device_ctx.device, &view_ci, nullptr, &ctx.image_views[i]);
        if (result != VK_SUCCESS) {
            // Clean up previously created views.
            for (uint32_t j = 0; j < i; ++j) {
                vkDestroyImageView(device_ctx.device, ctx.image_views[j], nullptr);
            }
            vkDestroySwapchainKHR(device_ctx.device, ctx.swapchain, nullptr);
            return Result<SwapchainContext>::err(
                "vkCreateImageView failed for image " + std::to_string(i));
        }
    }

    spdlog::info("Swapchain created: {}x{}, {} images",
                 ctx.extent.width, ctx.extent.height, image_count);
    return Result<SwapchainContext>::ok(std::move(ctx));
}

void destroy_swapchain(VkDevice device, SwapchainContext& ctx) {
    for (auto view : ctx.image_views) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    ctx.image_views.clear();
    ctx.images.clear();

    if (ctx.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, ctx.swapchain, nullptr);
        ctx.swapchain = VK_NULL_HANDLE;
        spdlog::info("Swapchain destroyed");
    }
}

} // namespace odyssey::vulkan
