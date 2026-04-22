#include "editor/editor.h"

#include "editor/project_paths.h"
#include "editor/scene_tree_panel.h"
#include "editor/inspector_panel.h"
#include "editor/viewport_panel.h"
#include "editor/log_panel.h"
#include "editor/asset_browser_panel.h"
#include "editor/scene_viewport_renderer.h"
#include "editor/file_dialog_win32.h"
#include "editor/editor_prefs.h"
#include "editor/play_snapshot.h"
#include "editor/undo_stack.h"
#include "editor/layout_presets.h"
#include "editor/entity_clipboard.h"
#include "editor/build_settings_panel.h"
#include "editor/preferences_panel.h"
#include "editor/status_bar.h"
#include "editor/command_palette.h"
#include "editor/asset_import.h"

#include "scene/scene_loader.h"
#include "scene/scene_serializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#define NOMINMAX
#include <windows.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
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

    // Phase 4: hold the parsed SceneData in the editor so the Inspector's
    // edit path can mirror mutations into it (the serializer needs the
    // preserved-unknown buckets + preserved_source snapshot on save).
    scene::SceneData scene_data;
    bool             has_scene_data = false;

    // Pointer to the log panel's sink installer (so Editor::initialize can
    // install the sink before the scene-loader starts logging).
    LogPanel* log_panel = nullptr;

    // Phase 2: live viewport scene renderer.
    SceneViewportRenderer viewport_renderer;
    bool                  has_viewport_renderer = false;
    VkDescriptorSet       viewport_ds = VK_NULL_HANDLE; // ImGui_ImplVulkan_AddTexture result
    VkExtent2D            viewport_extent{1280, 720};
    float                 camera_orbit_time = 0.0f;

    std::filesystem::path exe_dir;
    std::string imgui_ini_path;
    bool first_run_dock_build_pending = false;

    // Batch B: editor preferences (recent scenes, etc.)
    EditorPrefs editor_prefs;
    std::string cached_window_title;  // For efficient title-bar updates

    // Batch F: Play-in-Editor snapshot/restore
    std::optional<PlaySnapshot> play_snapshot;

    // Batch F: Undo/redo stacks
    UndoStack undo_stack;

    // Batch F: Scene dirty flag tracking (for undo capture)
    bool scene_dirty_prev = false;

    // Batch H: Splash screen timing (auto-dismiss after 1.5s)
    float splash_time = 0.0f;
    bool show_splash = true;

    // Batch H: First-run welcome wizard
    bool first_run_wizard_pending = false;
    int wizard_page = 0;  // 0=welcome, 1=quick-tour, 2=try-it

    // Batch H: About dialog flag
    bool show_about_dialog = false;

    // Batch H: Command palette state
    bool show_command_palette = false;
    std::string command_palette_query;
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    impl.imgui_ini_path = (impl.exe_dir / "imgui.ini").string();
    io.IniFilename = impl.imgui_ini_path.c_str();

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

    // Resolve absolute exe directory for imgui.ini + asset path resolution.
    {
        wchar_t buf[MAX_PATH] = {};
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n == 0 || n == MAX_PATH) {
            return Result<bool>::err("GetModuleFileNameW failed");
        }
        impl_->exe_dir = std::filesystem::path(buf).parent_path();
    }

    // First-run detection: if imgui.ini is absent, queue default-layout build.
    {
        std::error_code ec;
        bool ini_exists = std::filesystem::exists(impl_->exe_dir / "imgui.ini", ec);
        impl_->first_run_dock_build_pending = !ini_exists;
    }

    // Batch B: Load editor preferences (recent scenes, etc.)
    {
        auto prefs_res = load_editor_prefs(impl_->exe_dir);
        if (prefs_res.is_ok()) {
            impl_->editor_prefs = std::move(prefs_res).value();
        } else {
            spdlog::warn("[editor] load_editor_prefs failed: {}", prefs_res.error());
            // Continue with empty prefs on failure
        }
    }

    // Resolve absolute project paths (new editor/project_paths module).
    auto paths_res = editor::resolve_project_paths(impl_->exe_dir, scene_path);
    if (paths_res.is_err()) {
        return Result<bool>::err("resolve_project_paths: " + paths_res.error());
    }
    auto paths = std::move(paths_res).value();
    std::error_code cd_ec;
    std::filesystem::current_path(paths.exe_dir, cd_ec);
    if (cd_ec) {
        spdlog::warn("[editor] current_path({}) failed: {}",
                     paths.exe_dir.string(), cd_ec.message());
    }
    state_.scene_path   = paths.showcase_scene;
    state_.project_root = paths.asset_root;

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

    // Drop callback for drag-drop asset import
    glfwSetDropCallback(impl_->window,
        [](GLFWwindow* w, int count, const char** paths) {
            auto* self = static_cast<Editor*>(glfwGetWindowUserPointer(w));
            if (!self || !self->impl_ || count <= 0) return;

            for (int i = 0; i < count; ++i) {
                ImportSource source{std::filesystem::path(paths[i])};
                auto result = execute_import(source, self->state_.project_root, false);
                if (result.is_ok()) {
                    spdlog::info("[editor] dropped asset imported: {}", paths[i]);
                    // Trigger asset browser refresh on success
                    for (auto& panel : self->panels_) {
                        if (auto* browser = dynamic_cast<AssetBrowserPanel*>(panel.get())) {
                            browser->refresh(self->state_.project_root);
                        }
                    }
                } else {
                    spdlog::warn("[editor] drop import failed: {}", result.error());
                }
            }
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
            impl_->scene_data = std::move(scene_res).value();
            impl_->has_scene_data = true;
            scene::populate_entities(impl_->entities, impl_->scene_data);
            state_.scene_data = &impl_->scene_data;
            spdlog::info("[editor] scene loaded: {} entities", impl_->entities.entity_count());
            state_.status_line = "Loaded " + scene_path.filename().string() +
                                 " (" + std::to_string(impl_->entities.entity_count()) +
                                 " entities)";
        }
    } else {
        state_.status_line = "No scene specified.";
    }

    // Batch H: Check for first-run wizard and splash
    {
        impl_->show_splash = true;
        impl_->splash_time = 0.0f;

        // First-run detection: if editor_prefs.xml does NOT exist, show wizard
        std::filesystem::path prefs_file = impl_->exe_dir / "editor_prefs.xml";
        if (!std::filesystem::exists(prefs_file)) {
            impl_->first_run_wizard_pending = true;
            impl_->wizard_page = 0;
            spdlog::info("[editor] first-run wizard queued");
        }
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
    panels_.push_back(std::make_unique<AssetBrowserPanel>());
    panels_.push_back(std::move(log_panel));
    panels_.push_back(std::make_unique<BuildSettingsPanel>());
    panels_.push_back(std::make_unique<PreferencesPanel>(impl_->exe_dir));
}

// Phase 4: consume EditorState save/swap requests between frames so we never
// mutate the EntityManager while a command buffer is in flight and so panels
// don't have to know about SceneData / SceneLoader directly.
static void handle_phase4_requests(Editor::Impl& impl, EditorState& state) {
    // --- Save requested ---
    if (state.save_requested) {
        state.save_requested = false;
        if (!impl.has_scene_data) {
            spdlog::warn("[editor] save requested but no scene is loaded");
        } else if (state.scene_path.empty()) {
            spdlog::warn("[editor] save requested but scene_path is empty");
        } else if (state.mode != Mode::Edit) {
            spdlog::warn("[editor] save refused — not in Edit mode (current={})",
                         std::string{mode_label(state.mode)});
        } else {
            auto r = scene::serialize_scene(impl.scene_data, state.scene_path);
            if (r.is_err()) {
                spdlog::error("[editor] save failed: {}", r.error());
                state.status_line = "Save failed: " + r.error();
            } else {
                // Best-effort stat the written file for a byte-count toast.
                std::error_code ec;
                auto sz = std::filesystem::file_size(state.scene_path, ec);
                const std::string msg = "Saved " +
                    state.scene_path.filename().string() + " (" +
                    (ec ? std::string{"?"} : std::to_string(sz)) + " bytes)";
                spdlog::info("[editor] {}", msg);
                state.status_line = msg;
                // The written file IS now the canonical source. Re-snapshot
                // it so a subsequent unmutated round-trip still byte-matches.
                std::ifstream f(state.scene_path,
                                std::ios::in | std::ios::binary);
                if (f.is_open()) {
                    std::ostringstream ss; ss << f.rdbuf();
                    impl.scene_data.preserved_source = ss.str();
                    impl.scene_data.mutated = false;
                    state.scene_dirty = false;  // Batch B: sync dirty flag
                }
                // Batch B: update recent scenes list
                auto& recent = impl.editor_prefs.recent_scenes;
                std::string path_str = state.scene_path.string();
                auto it = std::find(recent.begin(), recent.end(), path_str);
                if (it != recent.end()) {
                    recent.erase(it);  // Remove old position
                }
                recent.insert(recent.begin(), path_str);  // Insert at front
                if (recent.size() > 8) {
                    recent.resize(8);  // Keep only 8 most recent
                }
                // Save prefs to disk
                auto save_r = save_editor_prefs(impl.exe_dir, impl.editor_prefs);
                if (save_r.is_err()) {
                    spdlog::warn("[editor] failed to save editor prefs: {}", save_r.error());
                }
            }
        }
    }

    // --- Scene swap requested ---
    if (!state.scene_swap_request.empty()) {
        auto req = state.scene_swap_request;
        state.scene_swap_request.clear();
        if (state.mode != Mode::Edit) {
            spdlog::warn("[editor] scene swap refused — not in Edit mode");
        } else {
            auto r = scene::load_scene_file(req);
            if (r.is_err()) {
                spdlog::error("[editor] scene swap load failed: {}", r.error());
            } else {
                impl.entities.clear();
                impl.scene_data = std::move(r).value();
                impl.has_scene_data = true;
                scene::populate_entities(impl.entities, impl.scene_data);
                state.selected_entity = INVALID_ENTITY;
                state.multi_selected.clear();  // Batch B
                state.scene_path = req;
                state.scene_data = &impl.scene_data;
                state.scene_dirty = impl.scene_data.mutated;  // Batch B: sync dirty flag
                state.selected_asset.clear();
                spdlog::info("[editor] scene swapped to '{}' ({} entities)",
                             req.string(), impl.entities.entity_count());
                state.status_line = "Loaded " + req.filename().string() +
                                    " (" + std::to_string(impl.entities.entity_count()) +
                                    " entities)";
            }
        }
    }

    // Batch B: sync scene_dirty from SceneData::mutated (for changes made
    // by the Inspector or other panels)
    if (impl.has_scene_data) {
        state.scene_dirty = impl.scene_data.mutated;
    }
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

        // Ctrl+S — the shortcut MUST be polled through ImGui's IO (not GLFW
        // directly) so that typing Ctrl+S into an InputText still reaches
        // the active widget and does not trigger a save.
        {
            ImGuiIO& io = ImGui::GetIO();
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_S, false) &&
                !io.WantCaptureKeyboard) {
                state_.save_requested = true;
            }
        }

        for (auto& p : panels_) p->tick(dt);

        handle_phase4_requests(*impl_, state_);

        // Batch B: Update window title with scene name and dirty indicator
        if (impl_->has_scene_data && !state_.scene_path.empty()) {
            std::string scene_name = state_.scene_path.filename().string();
            std::string title = "OdysseyEngine Editor — " + scene_name;
            if (state_.scene_dirty) title += " [*]";
            title += "  (" + state_.scene_path.string() + ")";
            if (impl_->cached_window_title != title) {
                glfwSetWindowTitle(impl_->window, title.c_str());
                impl_->cached_window_title = title;
            }
        } else {
            std::string title = "OdysseyEngine Editor";
            if (impl_->cached_window_title != title) {
                glfwSetWindowTitle(impl_->window, title.c_str());
                impl_->cached_window_title = title;
            }
        }

        draw_frame(dt);
    }
    vkDeviceWaitIdle(impl_->device);
}

// Batch B: Draw the "Unsaved Changes" modal popup. Called from draw_frame()
// during ImGui rendering. Context param used to distinguish which action
// triggered the popup (new, open, recent, quit).
static void draw_unsaved_changes_popup(EditorState& state, const char* context) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. What would you like to do?");
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            state.save_requested = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            // The context-specific action will be re-triggered by the menu
            // caller on the next frame. For now, just close the popup and
            // clear the dirty flag.
            state.scene_dirty = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void Editor::draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        // --- New Scene (Ctrl+N) ---
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::MenuItem("New Scene", "Ctrl+N") ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_N, false) &&
             !io.WantCaptureKeyboard)) {
            if (state_.scene_dirty) {
                ImGui::OpenPopup("Unsaved Changes##new");
            } else {
                // Create empty scene
                impl_->entities.clear();
                impl_->scene_data = scene::SceneData();
                impl_->has_scene_data = true;
                state_.scene_path.clear();
                state_.scene_dirty = false;
                state_.selected_entity = INVALID_ENTITY;
                state_.multi_selected.clear();
                state_.scene_data = &impl_->scene_data;
                spdlog::info("[editor] new scene created");
            }
        }

        // --- Open Scene (Ctrl+O) ---
        if (ImGui::MenuItem("Open Scene", "Ctrl+O") ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_O, false) &&
             !io.WantCaptureKeyboard)) {
            if (state_.scene_dirty) {
                ImGui::OpenPopup("Unsaved Changes##open");
            } else {
                auto path = open_scene_dialog();
                if (path) {
                    state_.scene_swap_request = path.value();
                }
            }
        }

        ImGui::Separator();

        // --- Import Asset ---
        if (ImGui::MenuItem("Import Asset...", nullptr)) {
            // Win32 dialog to open any file type.
            OPENFILENAMEA ofn{};
            char filename[MAX_PATH]{};
            filename[0] = '\0';

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;  // GLFW doesn't expose Win32 HWND; dialog will appear independently
            ofn.lpstrFilter = "All Files\0*.*\0OBJ Files\0*.obj\0PNG Files\0*.png\0"
                              "JPEG Files\0*.jpg;*.jpeg\0WAV Files\0*.wav\0"
                              "GLB Files\0*.glb\0FBX Files\0*.fbx\0XML Files\0*.xml\0\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = sizeof(filename);
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn)) {
                ImportSource source{std::filesystem::path(filename)};
                auto result = execute_import(source, state_.project_root, false);
                if (result.is_ok()) {
                    spdlog::info("[editor] asset imported: {}", filename);
                    state_.status_line = "Imported " + std::filesystem::path(filename).filename().string();
                    // Refresh asset browser
                    for (auto& panel : panels_) {
                        if (auto* browser = dynamic_cast<AssetBrowserPanel*>(panel.get())) {
                            browser->refresh(state_.project_root);
                        }
                    }
                } else {
                    spdlog::error("[editor] import failed: {}", result.error());
                    state_.status_line = "Import failed: " + result.error();
                }
            }
        }

        ImGui::Separator();

        const bool can_save = impl_->has_scene_data &&
                              state_.mode == Mode::Edit;

        // --- Save Scene (Ctrl+S) ---
        if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, can_save)) {
            state_.save_requested = true;
        }

        // --- Save Scene As (Ctrl+Shift+S) ---
        if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S", false, can_save)) {
            auto path = save_scene_dialog(state_.scene_path.filename().string());
            if (path) {
                state_.scene_path = path.value();
                state_.save_requested = true;
            }
        }

        ImGui::Separator();

        // --- Recent Scenes submenu ---
        if (ImGui::BeginMenu("Recent Scenes", !impl_->editor_prefs.recent_scenes.empty())) {
            for (size_t i = 0; i < impl_->editor_prefs.recent_scenes.size(); ++i) {
                const auto& scene_path = impl_->editor_prefs.recent_scenes[i];
                std::string label = std::filesystem::path(scene_path).filename().string();
                if (i < 8) label = std::to_string(i + 1) + ". " + label;
                if (ImGui::MenuItem(label.c_str())) {
                    if (state_.scene_dirty) {
                        ImGui::OpenPopup("Unsaved Changes##recent");
                        // Store which recent scene to open in a temp variable
                        // (handled in the popup modal below)
                    } else {
                        state_.scene_swap_request = scene_path;
                    }
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) {
            if (state_.scene_dirty) {
                ImGui::OpenPopup("Unsaved Changes##quit");
            } else {
                glfwSetWindowShouldClose(impl_->window, GLFW_TRUE);
            }
        }
        ImGui::EndMenu();
    }

    // Batch F: Edit menu (Undo/Redo)
    if (ImGui::BeginMenu("Edit")) {
        ImGuiIO& io = ImGui::GetIO();
        bool can_undo = impl_->undo_stack.can_undo();
        bool can_redo = impl_->undo_stack.can_redo();

        // Ctrl+Z - Undo (with description if available)
        std::string undo_label = "Undo";
        if (can_undo) {
            if (const auto* entry = impl_->undo_stack.peek_undo()) {
                undo_label += " " + entry->description;
            }
        }
        if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, can_undo) ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Z, false) &&
             !io.WantCaptureKeyboard && can_undo)) {
            auto entry = impl_->undo_stack.pop_undo();
            auto restore_res = restore_snapshot(
                PlaySnapshot{entry.state, entry.entities},
                impl_->scene_data, impl_->entities);
            if (restore_res.is_ok()) {
                spdlog::info("[editor] undo: {}", entry.description);
            } else {
                spdlog::warn("[editor] undo failed: {}", restore_res.error());
            }
        }

        // Ctrl+Y - Redo (with description if available)
        std::string redo_label = "Redo";
        if (can_redo) {
            if (const auto* entry = impl_->undo_stack.peek_redo()) {
                redo_label += " " + entry->description;
            }
        }
        if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, can_redo) ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Y, false) &&
             !io.WantCaptureKeyboard && can_redo)) {
            auto entry = impl_->undo_stack.pop_redo();
            auto restore_res = restore_snapshot(
                PlaySnapshot{entry.state, entry.entities},
                impl_->scene_data, impl_->entities);
            if (restore_res.is_ok()) {
                spdlog::info("[editor] redo: {}", entry.description);
            } else {
                spdlog::warn("[editor] redo failed: {}", restore_res.error());
            }
        }

        // Cut (Ctrl+X)
        bool can_cut = state_.selected_entity != INVALID_ENTITY || !state_.multi_selected.empty();
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, can_cut) ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_X, false) &&
             !io.WantCaptureKeyboard && can_cut)) {
            // Copy selected entities to clipboard and mark as cut
            if (state_.selected_entity != INVALID_ENTITY) {
                if (auto* e = impl_->entities.get_entity(state_.selected_entity)) {
                    clipboard_copy_entity(*e);
                    entity_clipboard().is_cut = true;
                    impl_->entities.destroy_entity(state_.selected_entity);
                    state_.selected_entity = INVALID_ENTITY;
                }
            }
            spdlog::info("[editor] cut");
        }

        // Copy (Ctrl+C)
        bool can_copy = state_.selected_entity != INVALID_ENTITY || !state_.multi_selected.empty();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, can_copy) ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_C, false) &&
             !io.WantCaptureKeyboard && can_copy)) {
            if (state_.selected_entity != INVALID_ENTITY) {
                if (auto* e = impl_->entities.get_entity(state_.selected_entity)) {
                    clipboard_copy_entity(*e);
                }
            }
            spdlog::info("[editor] copy");
        }

        // Paste (Ctrl+V)
        bool can_paste = !entity_clipboard().entities.empty();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, can_paste) ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_V, false) &&
             !io.WantCaptureKeyboard && can_paste)) {
            // Paste entities from clipboard
            for (const auto& [id, entity] : entity_clipboard().entities) {
                // Create new entity with cloned data
                auto new_id = impl_->entities.create_entity(entity.name, entity.archetype);
                auto* new_entity = impl_->entities.get_entity(new_id);
                if (new_entity) {
                    new_entity->components = entity.components;
                }
            }
            if (entity_clipboard().is_cut) {
                clipboard_clear();
            }
            spdlog::info("[editor] paste");
        }

        ImGui::Separator();

        // Select All (Ctrl+A)
        if (ImGui::MenuItem("Select All", "Ctrl+A") ||
            ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_A, false) &&
             !io.WantCaptureKeyboard)) {
            state_.multi_selected.clear();
            for (const auto& [id, entity] : impl_->entities.get_all_entities()) {
                state_.multi_selected.insert(id);
            }
            spdlog::info("[editor] select all ({} entities)", state_.multi_selected.size());
        }

        // Deselect All (Esc)
        if (ImGui::MenuItem("Deselect All", "Esc") ||
            (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !io.WantCaptureKeyboard)) {
            state_.selected_entity = INVALID_ENTITY;
            state_.multi_selected.clear();
            spdlog::info("[editor] deselect all");
        }

        ImGui::Separator();

        // Batch G: Preferences (opens PreferencesPanel)
        if (ImGui::MenuItem("Preferences", nullptr)) {
            // PreferencesPanel is already created in build_panels().
            // Just make it visible.
            for (auto& p : panels_) {
                if (p->name() == "Preferences") {
                    p->set_visible(true);
                    break;
                }
            }
        }

        ImGui::EndMenu();
    }

    // Batch F: Window menu (Layout presets)
    if (ImGui::BeginMenu("Window")) {
        if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Default")) {
                // Apply Default preset
                spdlog::info("[editor] layout: Default");
                impl_->editor_prefs.active_layout = "Default";
            }
            if (ImGui::MenuItem("2-by-3")) {
                spdlog::info("[editor] layout: 2-by-3");
                impl_->editor_prefs.active_layout = "2-by-3";
            }
            if (ImGui::MenuItem("Tall")) {
                spdlog::info("[editor] layout: Tall");
                impl_->editor_prefs.active_layout = "Tall";
            }
            if (ImGui::MenuItem("Wide")) {
                spdlog::info("[editor] layout: Wide");
                impl_->editor_prefs.active_layout = "Wide";
            }
            ImGui::EndMenu();
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
        // Batch H: About dialog
        if (ImGui::MenuItem("About")) {
            impl_->show_about_dialog = true;
        }

        ImGui::Separator();

        // Batch H: Help documentation (stubs for now — open in default app in Batch I)
        if (ImGui::MenuItem("Architecture")) {
            spdlog::info("[editor] Help: Open Architecture doc (deferred to Batch I)");
        }

        if (ImGui::MenuItem("Nadir Guide")) {
            spdlog::info("[editor] Help: Open Nadir Guide (deferred to Batch I)");
        }

        if (ImGui::MenuItem("CLI Reference")) {
            spdlog::info("[editor] Help: Open CLI Reference (deferred to Batch I)");
        }

        ImGui::EndMenu();
    }

    // Mode chip, right-aligned
    float chip_w = 320.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - chip_w);
    draw_mode_toolbar();

    ImGui::EndMainMenuBar();
}

void Editor::draw_mode_toolbar() {
    // Batch F: Compact 5-button toolbar with mode pill
    // Buttons: Play, Stop, Pause, Step, Simulate; right-align mode pill

    bool in_play_or_simulate = (state_.mode == Mode::Play || state_.mode == Mode::Simulate);

    // Play button (green when active)
    if (ImGui::Button("Play##mode_play")) {
        if (state_.mode != Mode::Play) {
            // Capture snapshot before switching to Play mode
            auto snap = capture_snapshot(impl_->scene_data, impl_->entities);
            if (snap.is_ok()) {
                impl_->play_snapshot = snap.value();
                state_.mode = Mode::Play;
                state_.play_paused = false;
                state_.dirty = true;
                spdlog::info("[editor] mode: Play (snapshot captured)");
            } else {
                spdlog::warn("[editor] failed to capture Play snapshot: {}", snap.error());
            }
        }
    }
    ImGui::SameLine();

    // Stop button (only visible when not in Edit mode)
    if (in_play_or_simulate) {
        if (ImGui::Button("Stop##mode_stop")) {
            // Restore snapshot
            if (impl_->play_snapshot) {
                auto res = restore_snapshot(
                    impl_->play_snapshot.value(),
                    impl_->scene_data,
                    impl_->entities);
                if (res.is_ok()) {
                    impl_->play_snapshot.reset();
                    state_.mode = Mode::Edit;
                    state_.play_paused = false;
                    state_.dirty = true;
                    spdlog::info("[editor] mode: Edit (snapshot restored)");
                } else {
                    spdlog::warn("[editor] failed to restore snapshot: {}", res.error());
                }
            } else {
                spdlog::warn("[editor] no play snapshot to restore");
            }
        }
        ImGui::SameLine();
    }

    // Pause button (only visible when in Play/Simulate)
    if (in_play_or_simulate) {
        bool is_paused = state_.play_paused;
        if (is_paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImColor(100, 100, 100).Value);
        }
        if (ImGui::Button("Pause##mode_pause")) {
            state_.play_paused = !state_.play_paused;
            spdlog::info("[editor] pause: {}", state_.play_paused ? "on" : "off");
        }
        if (is_paused) ImGui::PopStyleColor();
        ImGui::SameLine();

        // Step button (only visible when paused)
        if (is_paused) {
            if (ImGui::Button("Step##mode_step")) {
                // Fire-once flag: set a flag so the engine advances one frame
                spdlog::info("[editor] step (1 frame)");
                // TODO: wire this to the engine's frame-advance logic
            }
            ImGui::SameLine();
        }
    }

    // Simulate button (orange)
    if (ImGui::Button("Simulate##mode_simulate")) {
        if (state_.mode != Mode::Simulate) {
            // Capture snapshot before switching to Simulate mode
            auto snap = capture_snapshot(impl_->scene_data, impl_->entities);
            if (snap.is_ok()) {
                impl_->play_snapshot = snap.value();
                state_.mode = Mode::Simulate;
                state_.play_paused = false;
                state_.dirty = true;
                spdlog::info("[editor] mode: Simulate (snapshot captured)");
            } else {
                spdlog::warn("[editor] failed to capture Simulate snapshot: {}", snap.error());
            }
        }
    }

    // Mode pill (right-aligned)
    ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
    ImU32 pill_color;
    const char* pill_text;
    switch (state_.mode) {
    case Mode::Edit:
        pill_color = IM_COL32(30, 144, 255, 255);  // Blue
        pill_text = "Edit";
        break;
    case Mode::Play:
        pill_color = IM_COL32(76, 175, 80, 255);   // Green
        pill_text = "Play";
        break;
    case Mode::Simulate:
        pill_color = IM_COL32(255, 152, 0, 255);   // Orange
        pill_text = "Simulate";
        break;
    default:
        pill_color = IM_COL32(128, 128, 128, 255);
        pill_text = "Unknown";
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImColor(pill_color).Value);
    ImGui::Button(pill_text, ImVec2(120, 0));
    ImGui::PopStyleColor();
}

void Editor::draw_status_bar() {
    // Compute FPS from last frame's delta_time using EMA with α=0.1.
    float current_fps = (last_delta_time_ > 0) ? (1.0f / last_delta_time_) : 60.0f;
    fps_ema_ = compute_fps_ema(fps_ema_, current_fps, 0.1f);

    odyssey::editor::draw_status_bar(state_, fps_ema_, last_delta_time_ * 1000.0f);
}

void Editor::draw_frame(float delta_time) {
    // Store delta_time for status bar FPS calculation.
    last_delta_time_ = delta_time;

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

    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(
        0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (impl_->first_run_dock_build_pending) {
        impl_->first_run_dock_build_pending = false;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(
            dock_main, ImGuiDir_Left, 0.20f, nullptr, &dock_main);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(
            dock_main, ImGuiDir_Right, 0.3125f, nullptr, &dock_main);
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(
            dock_main, ImGuiDir_Down, 0.25f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Scene Tree",    dock_left);
        ImGui::DockBuilderDockWindow("Inspector",     dock_right);
        ImGui::DockBuilderDockWindow("Viewport",      dock_main);
        ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom);
        ImGui::DockBuilderDockWindow("Log",           dock_bottom);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    draw_menu_bar();

    // ---- Batch D: Update free-fly scene camera (Edit mode only) ----
    if (state_.mode == Mode::Edit && impl_->has_viewport_renderer) {
        ImGuiIO& io = ImGui::GetIO();

        // Build input for scene camera.
        SceneCameraInput cam_input{};
        cam_input.move_forward = ImGui::IsKeyDown(ImGuiKey_W);
        cam_input.move_backward = ImGui::IsKeyDown(ImGuiKey_S);
        cam_input.move_left = ImGui::IsKeyDown(ImGuiKey_A);
        cam_input.move_right = ImGui::IsKeyDown(ImGuiKey_D);
        cam_input.move_up = ImGui::IsKeyDown(ImGuiKey_E) && !ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
        cam_input.move_down = ImGui::IsKeyDown(ImGuiKey_Q) && !ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
        cam_input.right_button_held = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        cam_input.mouse_dx = io.MouseDelta.x;
        cam_input.mouse_dy = io.MouseDelta.y;
        cam_input.shift_held = io.KeyShift;
        cam_input.scroll_delta = io.MouseWheel;

        // Update camera state.
        state_.viewport_camera = update_scene_camera(state_.viewport_camera, cam_input, delta_time);

        // Batch D: Hotkeys for gizmo mode (only when viewport has focus).
        // Q → Select, W → Translate, E → Rotate, R → Scale, T → Universal.
        // Only respond when not typing in a text field.
        if (!io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) state_.gizmo_mode = GizmoMode::Select;
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) state_.gizmo_mode = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) state_.gizmo_mode = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) state_.gizmo_mode = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_T, false)) state_.gizmo_mode = GizmoMode::Universal;

            // X → toggle Local/World space.
            if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
                state_.gizmo_space = (state_.gizmo_space == GizmoSpace::Local)
                    ? GizmoSpace::World : GizmoSpace::Local;
            }

            // F → frame selected entity.
            if (ImGui::IsKeyPressed(ImGuiKey_F, false) && state_.selected_entity != INVALID_ENTITY) {
                if (const auto* entity = state_.entities->get_entity(state_.selected_entity)) {
                    float radius = 1.0f;  // Default; MeshCollider support is Batch J.
                    auto target = compute_frame_target(entity->components.transform.position, radius);
                    state_.viewport_camera.position = target.position;
                    state_.viewport_camera.yaw = target.yaw;
                    state_.viewport_camera.pitch = target.pitch;
                }
            }
        }
    }

    for (auto& p : panels_) {
        p->draw(state_);
    }

    // Batch G: Draw status bar.
    draw_status_bar();

    // Batch B: Draw unsaved-changes popup
    draw_unsaved_changes_popup(state_, "action");

    // Batch H: Ctrl+P command palette
    if (ImGui::IsKeyPressed(ImGuiKey_P, false) && ImGui::GetIO().KeyCtrl) {
        impl_->show_command_palette = true;
        impl_->command_palette_query = "";
    }

    if (impl_->show_command_palette) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.3f));
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

        if (ImGui::BeginPopupModal("##command_palette", &impl_->show_command_palette,
                                   ImGuiWindowFlags_NoTitleBar)) {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##palette_input", impl_->command_palette_query.data(),
                           impl_->command_palette_query.capacity() + 1);

            ImGui::Separator();

            // Build command registry and filter
            CommandRegistry reg;
            register_builtin_commands(reg);
            auto filtered = filter_commands(reg.items, impl_->command_palette_query);

            // Draw filtered results
            if (ImGui::BeginListBox("##palette_list", ImVec2(-1, 300))) {
                for (size_t i = 0; i < filtered.size(); ++i) {
                    const auto* cmd = filtered[i];
                    bool selected = (i == 0);  // Top result is default selection
                    if (ImGui::Selectable(cmd->label.c_str(), selected)) {
                        if (cmd->invoke) cmd->invoke(state_);
                        impl_->show_command_palette = false;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && selected) {
                        if (cmd->invoke) cmd->invoke(state_);
                        impl_->show_command_palette = false;
                    }
                }
                ImGui::EndListBox();
            }

            ImGui::EndPopup();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            impl_->show_command_palette = false;
        }
    }

    // Batch H: Splash screen (auto-dismiss after 1.5s)
    if (impl_->show_splash) {
        impl_->splash_time += delta_time;
        if (impl_->splash_time > 1.5f || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            impl_->show_splash = false;
        } else {
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::BeginPopupModal("##splash", nullptr, ImGuiWindowFlags_NoTitleBar |
                                                        ImGuiWindowFlags_NoMove |
                                                        ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("OdysseyEngine Editor");
            ImGui::Text("Build: %s", __DATE__);
            ImGui::EndPopup();
        }
    }

    // Batch H: About dialog
    if (impl_->show_about_dialog) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("About OdysseyEngine", &impl_->show_about_dialog,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("OdysseyEngine Editor\n\n"
                             "Version: dev (Phase 8)\n"
                             "Build Date: %s\n"
                             "Git Hash: (unavailable in Batch H)\n\n"
                             "Licensed under MIT",
                             __DATE__);
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                impl_->show_about_dialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Batch H: First-run welcome wizard
    if (impl_->first_run_wizard_pending) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_FirstUseEver);

        if (ImGui::BeginPopupModal("Welcome to OdysseyEngine", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            if (impl_->wizard_page == 0) {
                ImGui::TextWrapped("Welcome to OdysseyEngine Editor!\n\n"
                                 "This is a powerful tool for crafting 3D game worlds\n"
                                 "with GPU-driven AI behavior authoring.\n\n"
                                 "Let's get you started.");
                ImGui::Spacing();
                ImGui::Spacing();
                if (ImGui::Button("Next >>", ImVec2(120, 0))) {
                    impl_->wizard_page = 1;
                }
            } else if (impl_->wizard_page == 1) {
                ImGui::TextWrapped("Editor Panels:\n\n"
                                 "• Hierarchy: Left — organize your scene entities\n"
                                 "• Inspector: Right — edit entity properties\n"
                                 "• Viewport: Center — 3D view of your scene\n"
                                 "• Asset Browser: Bottom — browse meshes, materials, etc.\n"
                                 "• Log: Bottom — engine messages & debug output");
                ImGui::Spacing();
                if (ImGui::Button("<< Back", ImVec2(100, 0))) {
                    impl_->wizard_page = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("Next >>", ImVec2(100, 0))) {
                    impl_->wizard_page = 2;
                }
            } else if (impl_->wizard_page == 2) {
                ImGui::TextWrapped("Quick Tips:\n\n"
                                 "• Press F to frame the selected entity\n"
                                 "• W/E/R to switch Move/Rotate/Scale tools\n"
                                 "• Ctrl+P for the command palette\n"
                                 "• F11 to toggle fullscreen\n\n"
                                 "Ready to create?");
                ImGui::Spacing();
                if (ImGui::Button("<< Back", ImVec2(100, 0))) {
                    impl_->wizard_page = 1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Let's Go!", ImVec2(100, 0))) {
                    impl_->first_run_wizard_pending = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
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
        // Batch D: Use free-fly camera in Edit mode; auto-orbit in Play/Simulate.
        glm::mat4 view;
        glm::mat4 proj;
        auto ext = impl_->viewport_renderer.extent();
        float aspect = (ext.height > 0)
            ? (static_cast<float>(ext.width) / static_cast<float>(ext.height))
            : 1.0f;

        if (state_.mode == Mode::Edit) {
            // Free-fly camera for editor.
            view = state_.viewport_camera.view_matrix();
            proj = state_.viewport_camera.projection_matrix(aspect);
        } else {
            // Auto-orbit for Play/Simulate modes.
            if (state_.mode == Mode::Play || state_.mode == Mode::Simulate) {
                impl_->camera_orbit_time += delta_time;
            }
            const float r = 60.0f;
            const float a = impl_->camera_orbit_time * 0.05f;
            glm::vec3 eye(std::cos(a) * r, 25.0f, std::sin(a) * r);
            glm::vec3 at(0.0f, 1.0f, 0.0f);
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            view = glm::lookAt(eye, at, up);
            proj = glm::perspective(glm::radians(55.0f), aspect, 0.5f, 500.0f);
        }

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

    // Batch B: Save editor preferences before tearing down
    auto save_r = save_editor_prefs(impl_->exe_dir, impl_->editor_prefs);
    if (save_r.is_err()) {
        spdlog::warn("[editor] failed to save editor prefs on shutdown: {}",
                     save_r.error());
    }

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
