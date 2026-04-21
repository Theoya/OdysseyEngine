#include "editor/editor.h"

#include "editor/scene_tree_panel.h"
#include "editor/inspector_panel.h"
#include "editor/viewport_panel.h"
#include "editor/log_panel.h"
#include "editor/scene_viewport_renderer.h"

#include "scene/scene_loader.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Because Phase 1 only needs a window + ImGui (and not the full engine's
// frame graph), the editor owns a minimal Vulkan setup focused on driving
// ImGui's Vulkan backend. This lets us ship Phase 1 without refactoring the
// Engine's render loop.
//
// If this file ever needs to host the full engine, the plan is to lift the
// ImGui init into a post-processor stage inside Engine::process_frame(),
// and have Editor become an overlay callback instead. That's Phase 2+.
// ---------------------------------------------------------------------------

namespace odyssey::editor {

// ---------------------------------------------------------------------------
// Pure helpers (exported, tested)
// ---------------------------------------------------------------------------

std::string entity_display_label(const scene::Entity& entity) {
    // Pattern: "name  [id]"  — name alone when identical to "entity{id}".
    std::string s = entity.name.empty()
        ? (std::string("entity_") + std::to_string(entity.id))
        : entity.name;
    s += "  [";
    s += std::to_string(entity.id);
    s += "]";
    return s;
}

bool is_static_archetype(const std::string& archetype) {
    return archetype == "static" || archetype == "prop" ||
           archetype == "geometry";
}

// ---------------------------------------------------------------------------
// Editor::Impl — owns Vulkan/ImGui state.
// ---------------------------------------------------------------------------

struct Editor::Impl {
    GLFWwindow*       window         = nullptr;

    VkInstance        instance       = VK_NULL_HANDLE;
    VkPhysicalDevice  phys_device    = VK_NULL_HANDLE;
    VkDevice          device         = VK_NULL_HANDLE;
    VkQueue           gfx_queue      = VK_NULL_HANDLE;
    uint32_t          gfx_family     = 0;
    VkSurfaceKHR      surface        = VK_NULL_HANDLE;
    VkDescriptorPool  imgui_pool     = VK_NULL_HANDLE;
    VkRenderPass      render_pass    = VK_NULL_HANDLE;
    VkSwapchainKHR    swapchain      = VK_NULL_HANDLE;
    VkFormat          swapchain_fmt  = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR   swapchain_cs   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D        swapchain_ext{};
    std::vector<VkImage>       sc_images;
    std::vector<VkImageView>   sc_views;
    std::vector<VkFramebuffer> sc_framebuffers;

    VkCommandPool     cmd_pool       = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmd_buffers;
    std::vector<VkSemaphore>     image_available;
    std::vector<VkSemaphore>     render_finished;
    std::vector<VkFence>         in_flight;
    uint32_t                     frame_idx = 0;

    bool     framebuffer_resized = false;
    bool     has_imgui           = false;

    scene::EntityManager entities;

    // Pointer to the log panel's sink installer (so Editor::initialize can
    // install the sink before the scene-loader starts logging).
    LogPanel* log_panel = nullptr;

    // Phase 2: live viewport scene renderer.
    SceneViewportRenderer viewport_renderer;
    bool                  has_viewport_renderer = false;
    VkDescriptorSet       viewport_ds = VK_NULL_HANDLE; // ImGui_ImplVulkan_AddTexture result
    VkExtent2D            viewport_extent{1280, 720};
    float                 camera_orbit_time = 0.0f;
};

// ---------------------------------------------------------------------------
// Vulkan helpers (local static — very thin, single-use).
// ---------------------------------------------------------------------------

static bool check_vk(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        spdlog::error("[editor] Vulkan call {} failed: {}", what,
                      static_cast<int>(r));
        return false;
    }
    return true;
}

static bool create_vk_instance(Editor::Impl& impl) {
    VkApplicationInfo app{};
    app.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "OdysseyEditor";
    app.apiVersion       = VK_API_VERSION_1_3;

    uint32_t glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> exts(glfw_exts, glfw_exts + glfw_ext_count);

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &app;
    ci.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();

    return check_vk(vkCreateInstance(&ci, nullptr, &impl.instance),
                    "vkCreateInstance");
}

static bool pick_physical_device(Editor::Impl& impl) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(impl.instance, &count, nullptr);
    if (count == 0) {
        spdlog::error("[editor] No Vulkan-capable GPU found");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(impl.instance, &count, devices.data());

    // Prefer discrete GPU
    VkPhysicalDevice best = VK_NULL_HANDLE;
    int best_score = -1;
    for (auto d : devices) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(d, &p);
        int s = 0;
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) s += 1000;
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) s += 100;
        if (s > best_score) { best = d; best_score = s; }
    }
    impl.phys_device = best;
    return impl.phys_device != VK_NULL_HANDLE;
}

static bool find_graphics_family(Editor::Impl& impl) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(impl.phys_device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> fams(count);
    vkGetPhysicalDeviceQueueFamilyProperties(impl.phys_device, &count, fams.data());

    for (uint32_t i = 0; i < count; ++i) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(impl.phys_device, i, impl.surface, &present);
        if ((fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
            impl.gfx_family = i;
            return true;
        }
    }
    return false;
}

static bool create_vk_device(Editor::Impl& impl) {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = impl.gfx_family;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &priority;

    const char* swap_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkPhysicalDeviceFeatures feats{};

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = &swap_ext;
    dci.pEnabledFeatures        = &feats;

    if (!check_vk(vkCreateDevice(impl.phys_device, &dci, nullptr, &impl.device),
                  "vkCreateDevice")) return false;
    vkGetDeviceQueue(impl.device, impl.gfx_family, 0, &impl.gfx_queue);
    return true;
}

static bool create_swapchain(Editor::Impl& impl) {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(impl.phys_device, impl.surface, &caps);

    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(impl.phys_device, impl.surface, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(impl.phys_device, impl.surface, &fmt_count, fmts.data());

    VkSurfaceFormatKHR chosen = fmts[0];
    for (const auto& f : fmts) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }
    }
    impl.swapchain_fmt = chosen.format;
    impl.swapchain_cs  = chosen.colorSpace;

    int w = 0, h = 0;
    glfwGetFramebufferSize(impl.window, &w, &h);
    VkExtent2D extent{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    if (caps.currentExtent.width != UINT32_MAX) extent = caps.currentExtent;
    impl.swapchain_ext = extent;

    uint32_t min_images = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && min_images > caps.maxImageCount)
        min_images = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = impl.surface;
    sci.minImageCount    = min_images;
    sci.imageFormat      = chosen.format;
    sci.imageColorSpace  = chosen.colorSpace;
    sci.imageExtent      = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform     = caps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped          = VK_TRUE;

    if (!check_vk(vkCreateSwapchainKHR(impl.device, &sci, nullptr, &impl.swapchain),
                  "vkCreateSwapchainKHR")) return false;

    uint32_t img_count = 0;
    vkGetSwapchainImagesKHR(impl.device, impl.swapchain, &img_count, nullptr);
    impl.sc_images.resize(img_count);
    vkGetSwapchainImagesKHR(impl.device, impl.swapchain, &img_count, impl.sc_images.data());

    impl.sc_views.resize(img_count);
    for (uint32_t i = 0; i < img_count; ++i) {
        VkImageViewCreateInfo vci{};
        vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image    = impl.sc_images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format   = chosen.format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;
        if (!check_vk(vkCreateImageView(impl.device, &vci, nullptr, &impl.sc_views[i]),
                      "vkCreateImageView(swapchain)")) return false;
    }
    return true;
}

static bool create_render_pass(Editor::Impl& impl) {
    VkAttachmentDescription color{};
    color.format         = impl.swapchain_fmt;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rci{};
    rci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rci.attachmentCount = 1;
    rci.pAttachments    = &color;
    rci.subpassCount    = 1;
    rci.pSubpasses      = &sub;
    rci.dependencyCount = 1;
    rci.pDependencies   = &dep;

    return check_vk(vkCreateRenderPass(impl.device, &rci, nullptr, &impl.render_pass),
                    "vkCreateRenderPass");
}

static bool create_framebuffers(Editor::Impl& impl) {
    impl.sc_framebuffers.resize(impl.sc_views.size());
    for (size_t i = 0; i < impl.sc_views.size(); ++i) {
        VkFramebufferCreateInfo fci{};
        fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass      = impl.render_pass;
        fci.attachmentCount = 1;
        fci.pAttachments    = &impl.sc_views[i];
        fci.width           = impl.swapchain_ext.width;
        fci.height          = impl.swapchain_ext.height;
        fci.layers          = 1;
        if (!check_vk(vkCreateFramebuffer(impl.device, &fci, nullptr, &impl.sc_framebuffers[i]),
                      "vkCreateFramebuffer")) return false;
    }
    return true;
}

static bool create_cmd_and_sync(Editor::Impl& impl) {
    VkCommandPoolCreateInfo pci{};
    pci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = impl.gfx_family;
    if (!check_vk(vkCreateCommandPool(impl.device, &pci, nullptr, &impl.cmd_pool),
                  "vkCreateCommandPool")) return false;

    const uint32_t N = static_cast<uint32_t>(impl.sc_images.size());
    impl.cmd_buffers.resize(N);
    VkCommandBufferAllocateInfo aci{};
    aci.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    aci.commandPool        = impl.cmd_pool;
    aci.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    aci.commandBufferCount = N;
    if (!check_vk(vkAllocateCommandBuffers(impl.device, &aci, impl.cmd_buffers.data()),
                  "vkAllocateCommandBuffers")) return false;

    impl.image_available.resize(N);
    impl.render_finished.resize(N);
    impl.in_flight.resize(N);
    for (uint32_t i = 0; i < N; ++i) {
        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (!check_vk(vkCreateSemaphore(impl.device, &sci, nullptr, &impl.image_available[i]),
                      "vkCreateSemaphore(image_available)")) return false;
        if (!check_vk(vkCreateSemaphore(impl.device, &sci, nullptr, &impl.render_finished[i]),
                      "vkCreateSemaphore(render_finished)")) return false;
        if (!check_vk(vkCreateFence(impl.device, &fci, nullptr, &impl.in_flight[i]),
                      "vkCreateFence")) return false;
    }
    return true;
}

static bool create_imgui_pool(Editor::Impl& impl) {
    VkDescriptorPoolSize sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                 64 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       64 },
    };
    VkDescriptorPoolCreateInfo pci{};
    pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pci.maxSets       = 1024;
    pci.poolSizeCount = static_cast<uint32_t>(sizeof(sizes)/sizeof(sizes[0]));
    pci.pPoolSizes    = sizes;
    return check_vk(vkCreateDescriptorPool(impl.device, &pci, nullptr, &impl.imgui_pool),
                    "vkCreateDescriptorPool(imgui)");
}

// Forward-declared helpers used by swapchain recreation & viewport wiring.
static void destroy_swapchain_objects(Editor::Impl& impl);
static bool rebuild_swapchain(Editor::Impl& impl);

static bool init_imgui(Editor::Impl& impl) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Note: vcpkg's imgui does NOT ship the docking branch. Phase 1 uses
    // the default multi-window layout; the user drags windows where they
    // like. Phase 2 may swap to docking once we vendor imgui-docking.

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding  = 4.0f;
    style.FrameRounding   = 3.0f;
    style.ScrollbarSize   = 14.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);

    ImGui_ImplGlfw_InitForVulkan(impl.window, true);

    ImGui_ImplVulkan_InitInfo init{};
    init.Instance        = impl.instance;
    init.PhysicalDevice  = impl.phys_device;
    init.Device          = impl.device;
    init.QueueFamily     = impl.gfx_family;
    init.Queue           = impl.gfx_queue;
    init.DescriptorPool  = impl.imgui_pool;
    init.RenderPass      = impl.render_pass;
    init.MinImageCount   = static_cast<uint32_t>(impl.sc_images.size());
    init.ImageCount      = static_cast<uint32_t>(impl.sc_images.size());
    init.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;
    init.Subpass         = 0;

    if (!ImGui_ImplVulkan_Init(&init)) {
        spdlog::error("[editor] ImGui_ImplVulkan_Init failed");
        return false;
    }
    // Newer imgui_impl_vulkan builds fonts on first frame — explicit call
    // is optional and not always available. Skip it.
    impl.has_imgui = true;
    return true;
}

// Destroy only the swapchain-dependent objects (views, framebuffers, swapchain
// handle). Leaves the render pass, command pool, semaphores, and ImGui state
// intact — those are independent of swapchain extent.
static void destroy_swapchain_objects(Editor::Impl& impl) {
    auto dev = impl.device;
    for (auto& fb : impl.sc_framebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(dev, fb, nullptr);
    }
    impl.sc_framebuffers.clear();
    for (auto& v : impl.sc_views) {
        if (v != VK_NULL_HANDLE) vkDestroyImageView(dev, v, nullptr);
    }
    impl.sc_views.clear();
    if (impl.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(dev, impl.swapchain, nullptr);
        impl.swapchain = VK_NULL_HANDLE;
    }
}

// Re-create the swapchain and its framebuffers at the current GLFW framebuffer
// size. Handles the minimization case (size 0x0) by returning true without
// building anything — caller should early-out until size is nonzero.
static bool rebuild_swapchain(Editor::Impl& impl) {
    int w = 0, h = 0;
    glfwGetFramebufferSize(impl.window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(impl.window, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(impl.device);

    destroy_swapchain_objects(impl);

    if (!create_swapchain(impl))   return false;
    if (!create_framebuffers(impl)) return false;

    impl.frame_idx = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Editor lifecycle
// ---------------------------------------------------------------------------

Editor::Editor() : impl_(std::make_unique<Impl>()) {}

Editor::~Editor() {
    shutdown();
}

Result<bool> Editor::initialize(const std::filesystem::path& scene_path) {
    spdlog::info("[editor] initializing");

    build_panels();
    state_.entities   = &impl_->entities;
    state_.scene_path = scene_path;

    // Install log sink BEFORE anything else logs (subsequent spdlog calls
    // populate the editor's log panel buffer).
    if (impl_->log_panel) {
        impl_->log_panel->install_sink();
    }

    // -------- GLFW window --------
    if (!glfwInit()) {
        return Result<bool>::err("glfwInit failed");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    impl_->window = glfwCreateWindow(1600, 900, "OdysseyEditor", nullptr, nullptr);
    if (!impl_->window) {
        glfwTerminate();
        return Result<bool>::err("glfwCreateWindow failed");
    }
    glfwSetWindowUserPointer(impl_->window, this);
    glfwSetFramebufferSizeCallback(impl_->window,
        [](GLFWwindow* w, int, int) {
            auto* self = static_cast<Editor*>(glfwGetWindowUserPointer(w));
            if (self && self->impl_) self->impl_->framebuffer_resized = true;
        });

    // -------- Vulkan --------
    if (!create_vk_instance(*impl_))   return Result<bool>::err("Vulkan instance creation failed");
    if (!pick_physical_device(*impl_)) return Result<bool>::err("No Vulkan physical device");

    if (glfwCreateWindowSurface(impl_->instance, impl_->window, nullptr, &impl_->surface) != VK_SUCCESS) {
        return Result<bool>::err("glfwCreateWindowSurface failed");
    }

    if (!find_graphics_family(*impl_)) return Result<bool>::err("No graphics+present queue family");
    if (!create_vk_device(*impl_))     return Result<bool>::err("vkCreateDevice failed");
    if (!create_swapchain(*impl_))     return Result<bool>::err("swapchain creation failed");
    if (!create_render_pass(*impl_))   return Result<bool>::err("render pass creation failed");
    if (!create_framebuffers(*impl_))  return Result<bool>::err("framebuffer creation failed");
    if (!create_cmd_and_sync(*impl_))  return Result<bool>::err("cmd/sync creation failed");
    if (!create_imgui_pool(*impl_))    return Result<bool>::err("imgui descriptor pool failed");
    if (!init_imgui(*impl_))           return Result<bool>::err("ImGui init failed");

    // -------- Phase 2: Scene viewport renderer --------
    {
        SceneViewportInit vi{};
        vi.phys_device     = impl_->phys_device;
        vi.device          = impl_->device;
        vi.graphics_queue  = impl_->gfx_queue;
        vi.graphics_family = impl_->gfx_family;
        vi.initial_extent  = impl_->viewport_extent;
        vi.color_format    = VK_FORMAT_R8G8B8A8_UNORM; // stable for ImGui sample
        auto vr = impl_->viewport_renderer.initialize(vi);
        if (vr.is_err()) {
            spdlog::warn("[editor] viewport renderer init failed: {} — viewport panel will fall back to placeholder",
                         vr.error());
        } else {
            impl_->has_viewport_renderer = true;
            // Register the offscreen image with ImGui so it can sample it
            // as a regular texture from the panel. The returned descriptor
            // set is the ImTextureID.
            impl_->viewport_ds = ImGui_ImplVulkan_AddTexture(
                impl_->viewport_renderer.offscreen_sampler(),
                impl_->viewport_renderer.offscreen_view(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            state_.viewport_renderer = &impl_->viewport_renderer;
            state_.viewport_texture_id = static_cast<void*>(impl_->viewport_ds);
            spdlog::info("[editor] viewport renderer registered with ImGui");
        }
    }

    // -------- Scene load (non-fatal in Phase 1) --------
    if (!scene_path.empty()) {
        spdlog::info("[editor] loading scene: {}", scene_path.string());
        auto scene_res = scene::load_scene_file(scene_path);
        if (scene_res.is_err()) {
            spdlog::warn("[editor] scene load failed (non-fatal in Phase 1): {}",
                         scene_res.error());
            state_.status_line = "Scene load failed: " + scene_res.error();
        } else {
            auto scene_data = std::move(scene_res).value();
            scene::populate_entities(impl_->entities, scene_data);
            spdlog::info("[editor] scene loaded: {} entities", impl_->entities.entity_count());
            state_.status_line = "Loaded " + scene_path.filename().string() +
                                 " (" + std::to_string(impl_->entities.entity_count()) +
                                 " entities)";
        }
    } else {
        state_.status_line = "No scene specified.";
    }

    spdlog::info("[editor] initialized");
    return Result<bool>::ok(true);
}

void Editor::build_panels() {
    auto log_panel = std::make_unique<LogPanel>();
    impl_->log_panel = log_panel.get();

    panels_.push_back(std::make_unique<SceneTreePanel>());
    panels_.push_back(std::make_unique<InspectorPanel>());
    panels_.push_back(std::make_unique<ViewportPanel>());
    panels_.push_back(std::move(log_panel));
}

void Editor::run() {
    if (!impl_->window) return;
    double last = glfwGetTime();
    while (!glfwWindowShouldClose(impl_->window)) {
        glfwPollEvents();
        double now = glfwGetTime();
        float dt = static_cast<float>(now - last);
        last = now;
        if (dt > 0.25f) dt = 0.25f;

        for (auto& p : panels_) p->tick(dt);

        draw_frame(dt);
    }
    vkDeviceWaitIdle(impl_->device);
}

void Editor::draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Reload Scene", "Ctrl+R", false, false)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) {
            glfwSetWindowShouldClose(impl_->window, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        for (auto& p : panels_) {
            bool v = p->visible();
            if (ImGui::MenuItem(p->name().c_str(), nullptr, v)) {
                p->set_visible(!v);
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("OdysseyEditor — Phase 1", nullptr, false, false);
        ImGui::MenuItem("Read-only inspector. Phase 2 adds editing.",
                        nullptr, false, false);
        ImGui::EndMenu();
    }

    // Mode chip, right-aligned
    float chip_w = 320.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - chip_w);
    draw_mode_toolbar();

    ImGui::EndMainMenuBar();
}

void Editor::draw_mode_toolbar() {
    auto mode_button = [&](Mode m, ImU32 color) {
        bool is_current = (state_.mode == m);
        std::string_view label = mode_label(m);
        if (is_current) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImColor(color).Value);
        }
        std::string btn{label};
        if (ImGui::Button(btn.c_str())) {
            if (state_.mode != m) {
                state_.mode = m;
                state_.dirty = true;
                spdlog::info("[editor] mode: {}", btn);
            }
        }
        if (is_current) ImGui::PopStyleColor();
    };
    mode_button(Mode::Edit,     IM_COL32(60,  80, 160, 255));
    ImGui::SameLine();
    mode_button(Mode::Play,     IM_COL32(60, 160,  80, 255));
    ImGui::SameLine();
    mode_button(Mode::Simulate, IM_COL32(160, 120, 60, 255));
}

void Editor::draw_status_bar() {
    // Phase 1: just append status to the log panel's last line via spdlog —
    // skip a dedicated status window to keep the screen uncluttered.
}

void Editor::draw_frame(float delta_time) {
    // ---- Apply any viewport-resize request made last frame (outside any
    //      command buffer recording) before touching the fence.
    if (impl_->has_viewport_renderer &&
        state_.viewport_requested_width > 0 && state_.viewport_requested_height > 0 &&
        (state_.viewport_requested_width  != impl_->viewport_extent.width ||
         state_.viewport_requested_height != impl_->viewport_extent.height)) {
        VkExtent2D new_ext{state_.viewport_requested_width,
                           state_.viewport_requested_height};
        auto r = impl_->viewport_renderer.resize(new_ext);
        if (r.is_err()) {
            spdlog::warn("[editor] viewport resize failed: {}", r.error());
        } else {
            impl_->viewport_extent = new_ext;
            // The ImGui descriptor must be re-pointed at the new image view.
            // Free the old descriptor set and allocate a new one.
            if (impl_->viewport_ds != VK_NULL_HANDLE) {
                ImGui_ImplVulkan_RemoveTexture(impl_->viewport_ds);
                impl_->viewport_ds = VK_NULL_HANDLE;
            }
            impl_->viewport_ds = ImGui_ImplVulkan_AddTexture(
                impl_->viewport_renderer.offscreen_sampler(),
                impl_->viewport_renderer.offscreen_view(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            state_.viewport_texture_id = static_cast<void*>(impl_->viewport_ds);
        }
    }
    state_.viewport_requested_width = 0;
    state_.viewport_requested_height = 0;

    // ---- Wait for the frame's fence so we can reuse its cmd buffer ----
    vkWaitForFences(impl_->device, 1, &impl_->in_flight[impl_->frame_idx],
                    VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult acquire = vkAcquireNextImageKHR(
        impl_->device, impl_->swapchain, UINT64_MAX,
        impl_->image_available[impl_->frame_idx], VK_NULL_HANDLE, &image_index);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR || impl_->framebuffer_resized) {
        impl_->framebuffer_resized = false;
        // Phase 2: full swapchain recreation.
        if (!rebuild_swapchain(*impl_)) {
            spdlog::error("[editor] swapchain rebuild failed");
        }
        return;
    }

    vkResetFences(impl_->device, 1, &impl_->in_flight[impl_->frame_idx]);

    // ---- Begin ImGui frame ----
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_menu_bar();

    for (auto& p : panels_) {
        p->draw(state_);
    }

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    // ---- Record cmd buffer ----
    VkCommandBuffer cmd = impl_->cmd_buffers[impl_->frame_idx];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);

    // ---- Phase 2: record scene render into offscreen target FIRST ----
    // The scene render pass' final layout is SHADER_READ_ONLY_OPTIMAL with
    // an explicit subpass dependency (COLOR_ATTACHMENT_OUTPUT_BIT →
    // FRAGMENT_SHADER_BIT), so ImGui can sample the image safely when the
    // main render pass begins below — no manual pipeline barrier required.
    if (impl_->has_viewport_renderer) {
        // Advance camera orbit time only while in Play/Simulate modes so
        // Edit mode feels like a paused scene.
        if (state_.mode == Mode::Play || state_.mode == Mode::Simulate) {
            impl_->camera_orbit_time += delta_time;
        }

        // Build an orbit VP matrix. The liminal mood wants a slow arc —
        // orbit at radius 60, height 25, 0.05 rad/s.
        const float r = 60.0f;
        const float a = impl_->camera_orbit_time * 0.05f;
        glm::vec3 eye(std::cos(a) * r, 25.0f, std::sin(a) * r);
        glm::vec3 at(0.0f, 1.0f, 0.0f);
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::mat4 view = glm::lookAt(eye, at, up);
        auto ext = impl_->viewport_renderer.extent();
        float aspect = (ext.height > 0)
            ? (static_cast<float>(ext.width) / static_cast<float>(ext.height))
            : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(55.0f), aspect, 0.5f, 500.0f);
        proj[1][1] *= -1.0f; // flip Y for Vulkan's clip space
        glm::mat4 vp = proj * view;

        // Convert entities to draw records. Color by archetype for quick
        // visual scan (keeps the scene legible before Phase 4's materials).
        std::vector<SceneDrawEntity> draws;
        if (state_.entities) {
            draws.reserve(state_.entities->entity_count());
            for (const auto& [id, entity] : state_.entities->get_all_entities()) {
                SceneDrawEntity d{};
                const auto& t = entity.components.transform;
                d.position[0] = t.position.x;
                d.position[1] = t.position.y;
                d.position[2] = t.position.z;
                d.scale[0] = std::max(t.scale.x, 0.1f);
                d.scale[1] = std::max(t.scale.y, 0.1f);
                d.scale[2] = std::max(t.scale.z, 0.1f);
                // Archetype → color table. Stable palette within the mood.
                const std::string& a_name = entity.archetype;
                if      (a_name == "player")              { d.color[0]=0.80f; d.color[1]=0.90f; d.color[2]=1.00f; }
                else if (a_name == "static")              { d.color[0]=0.55f; d.color[1]=0.50f; d.color[2]=0.45f; }
                else if (a_name == "enemy_pack_hunter")   { d.color[0]=0.85f; d.color[1]=0.35f; d.color[2]=0.35f; }
                else if (a_name == "enemy_ranged")        { d.color[0]=0.90f; d.color[1]=0.55f; d.color[2]=0.30f; }
                else if (a_name == "multi_arm_gunner")    { d.color[0]=1.00f; d.color[1]=0.25f; d.color[2]=0.45f; }
                else if (a_name == "civilian")            { d.color[0]=0.75f; d.color[1]=0.75f; d.color[2]=0.85f; }
                else if (a_name == "light")               { d.color[0]=1.00f; d.color[1]=0.95f; d.color[2]=0.75f; }
                else if (a_name == "audio_source")        { d.color[0]=0.50f; d.color[1]=0.75f; d.color[2]=0.65f; }
                else if (a_name == "system")              { d.color[0]=0.30f; d.color[1]=0.30f; d.color[2]=0.35f; }
                else                                      { d.color[0]=0.60f; d.color[1]=0.60f; d.color[2]=0.70f; }
                d.color[3] = 1.0f;
                // Skip invisible/zero-scale entities (systems, music).
                if (d.scale[0] < 0.05f) continue;
                draws.push_back(d);
            }
        }

        // Pass VP as column-major float[16] (glm::mat4 is column-major).
        float vp_cm[16];
        const float* ptr = reinterpret_cast<const float*>(&vp[0][0]);
        for (int i = 0; i < 16; ++i) vp_cm[i] = ptr[i];

        impl_->viewport_renderer.record(cmd, vp_cm, draws);
    }

    VkClearValue clear{};
    clear.color = { {0.02f, 0.02f, 0.03f, 1.0f} };

    VkRenderPassBeginInfo rpbi{};
    rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass        = impl_->render_pass;
    rpbi.framebuffer       = impl_->sc_framebuffers[image_index];
    rpbi.renderArea.extent = impl_->swapchain_ext;
    rpbi.clearValueCount   = 1;
    rpbi.pClearValues      = &clear;

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    if (impl_->has_imgui && draw_data) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
    }
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // ---- Submit ----
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &impl_->image_available[impl_->frame_idx];
    submit.pWaitDstStageMask    = &wait_stage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &impl_->render_finished[impl_->frame_idx];
    vkQueueSubmit(impl_->gfx_queue, 1, &submit, impl_->in_flight[impl_->frame_idx]);

    // ---- Present ----
    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &impl_->render_finished[impl_->frame_idx];
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &impl_->swapchain;
    pi.pImageIndices      = &image_index;
    VkResult present = vkQueuePresentKHR(impl_->gfx_queue, &pi);

    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR ||
        impl_->framebuffer_resized) {
        impl_->framebuffer_resized = false;
        if (!rebuild_swapchain(*impl_)) {
            spdlog::error("[editor] swapchain rebuild failed after present");
        }
    }

    impl_->frame_idx = (impl_->frame_idx + 1) % static_cast<uint32_t>(impl_->cmd_buffers.size());
}

void Editor::shutdown() {
    if (!impl_) return;
    if (impl_->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(impl_->device);
    }

    // Viewport renderer first (ImGui may still be sampling it until shutdown).
    if (impl_->has_viewport_renderer) {
        if (impl_->has_imgui && impl_->viewport_ds != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(impl_->viewport_ds);
            impl_->viewport_ds = VK_NULL_HANDLE;
        }
        impl_->viewport_renderer.shutdown();
        impl_->has_viewport_renderer = false;
    }

    if (impl_->has_imgui) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        impl_->has_imgui = false;
    }

    auto dev = impl_->device;
    if (dev != VK_NULL_HANDLE) {
        for (auto& s : impl_->image_available) vkDestroySemaphore(dev, s, nullptr);
        for (auto& s : impl_->render_finished) vkDestroySemaphore(dev, s, nullptr);
        for (auto& f : impl_->in_flight)       vkDestroyFence(dev, f, nullptr);
        impl_->image_available.clear();
        impl_->render_finished.clear();
        impl_->in_flight.clear();

        if (impl_->cmd_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(dev, impl_->cmd_pool, nullptr);
            impl_->cmd_pool = VK_NULL_HANDLE;
        }
        for (auto& fb : impl_->sc_framebuffers) {
            if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(dev, fb, nullptr);
        }
        impl_->sc_framebuffers.clear();
        for (auto& v : impl_->sc_views) {
            if (v != VK_NULL_HANDLE) vkDestroyImageView(dev, v, nullptr);
        }
        impl_->sc_views.clear();
        if (impl_->swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(dev, impl_->swapchain, nullptr);
            impl_->swapchain = VK_NULL_HANDLE;
        }
        if (impl_->render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(dev, impl_->render_pass, nullptr);
            impl_->render_pass = VK_NULL_HANDLE;
        }
        if (impl_->imgui_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(dev, impl_->imgui_pool, nullptr);
            impl_->imgui_pool = VK_NULL_HANDLE;
        }
        vkDestroyDevice(dev, nullptr);
        impl_->device = VK_NULL_HANDLE;
    }
    if (impl_->surface != VK_NULL_HANDLE && impl_->instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(impl_->instance, impl_->surface, nullptr);
        impl_->surface = VK_NULL_HANDLE;
    }
    if (impl_->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(impl_->instance, nullptr);
        impl_->instance = VK_NULL_HANDLE;
    }
    if (impl_->window) {
        glfwDestroyWindow(impl_->window);
        impl_->window = nullptr;
        glfwTerminate();
    }
    impl_.reset();
}

} // namespace odyssey::editor
