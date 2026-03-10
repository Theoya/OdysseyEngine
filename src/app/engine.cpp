#include "app/engine.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <chrono>

// ---------------------------------------------------------------------------
// Vulkan / Nadir headers — pulled in only inside this translation unit.
// Phase 1: we include the interface headers that other agents will provide.
// Until those exist we guard with __has_include so the file compiles
// standalone for the CLI-only build.
// ---------------------------------------------------------------------------

#if __has_include("vulkan/instance.h")
#   include "vulkan/instance.h"
#   include "vulkan/device.h"
#   include "vulkan/swapchain.h"
#   include "vulkan/buffer.h"
#   include "vulkan/command.h"
#   include "vulkan/compute_pipeline.h"
#   define ODYSSEY_HAS_VULKAN 1
#else
#   define ODYSSEY_HAS_VULKAN 0
#endif

#if __has_include("nadir/nadir_system.h")
#   include "nadir/nadir_system.h"
#   define ODYSSEY_HAS_NADIR 1
#else
#   define ODYSSEY_HAS_NADIR 0
#endif

#if __has_include(<GLFW/glfw3.h>)
#   include <GLFW/glfw3.h>
#   define ODYSSEY_HAS_GLFW 1
#else
#   define ODYSSEY_HAS_GLFW 0
#endif

#if __has_include(<pugixml.hpp>)
#   include <pugixml.hpp>
#   define ODYSSEY_HAS_PUGIXML 1
#else
#   define ODYSSEY_HAS_PUGIXML 0
#endif

namespace odyssey {

// ---------------------------------------------------------------------------
// Pimpl — holds Vulkan and Nadir runtime state.
// ---------------------------------------------------------------------------

struct Engine::Impl {
#if ODYSSEY_HAS_VULKAN
    VkInstance                        instance        = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT          debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR                      surface         = VK_NULL_HANDLE;
    vulkan::DeviceContext             device_ctx{};
    vulkan::SwapchainContext          swapchain_ctx{};
    VkCommandPool                     command_pool    = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>      command_buffers;
    std::vector<vulkan::FrameSync>    frame_sync;
    VkDescriptorPool                  descriptor_pool = VK_NULL_HANDLE;
#endif

#if ODYSSEY_HAS_NADIR
    nadir::NadirSystem                nadir_system;
#endif
};

// ---------------------------------------------------------------------------
// parse_engine_config — pure XML parser
// ---------------------------------------------------------------------------

Result<EngineConfig> parse_engine_config(const std::filesystem::path& config_path) {
    EngineConfig config;

#if ODYSSEY_HAS_PUGIXML
    pugi::xml_document doc;
    pugi::xml_parse_result parse = doc.load_file(config_path.c_str());
    if (!parse) {
        return Result<EngineConfig>::err(
            "Failed to parse " + config_path.string() + ": " + parse.description());
    }

    auto root = doc.child("engine");
    if (!root) {
        return Result<EngineConfig>::err(
            "Missing <engine> root element in " + config_path.string());
    }

    // Window settings
    if (auto win = root.child("window")) {
        config.window_width  = win.attribute("width").as_uint(config.window_width);
        config.window_height = win.attribute("height").as_uint(config.window_height);
        config.window_title  = win.attribute("title").as_string(config.window_title.c_str());
        config.vsync         = win.attribute("vsync").as_bool(config.vsync);
    }

    // Vulkan settings
    if (auto vk = root.child("vulkan")) {
        config.validation_layers = vk.attribute("validation").as_bool(config.validation_layers);
        config.gpu_index         = vk.attribute("gpu_index").as_uint(config.gpu_index);
    }

    // Nadir settings
    if (auto nadir = root.child("nadir")) {
        config.behavior_dir = nadir.attribute("behavior_dir").as_string(config.behavior_dir.string().c_str());
        config.lib_dir      = nadir.attribute("lib_dir").as_string(config.lib_dir.string().c_str());
        config.hot_reload   = nadir.attribute("hot_reload").as_bool(config.hot_reload);
        config.max_agents   = nadir.attribute("max_agents").as_uint(config.max_agents);
    }
#else
    // Without pugixml, check that the file at least exists.
    if (!std::filesystem::exists(config_path)) {
        return Result<EngineConfig>::err(
            "Config file not found: " + config_path.string() +
            " (pugixml not available for parsing)");
    }
    spdlog::warn("pugixml not available — using default EngineConfig");
#endif

    return Result<EngineConfig>::ok(std::move(config));
}

// ---------------------------------------------------------------------------
// Engine lifetime
// ---------------------------------------------------------------------------

Engine::Engine()  = default;

Engine::~Engine() {
    if (running_) {
        shutdown();
    }
}

// ---------------------------------------------------------------------------
// initialize
// ---------------------------------------------------------------------------

Result<bool> Engine::initialize(const EngineConfig& config) {
    impl_ = std::make_unique<Impl>();

    spdlog::info("Initializing OdysseyEngine");
    spdlog::info("  Window   : {}x{} \"{}\"", config.window_width,
                 config.window_height, config.window_title);
    spdlog::info("  Vsync    : {}", config.vsync ? "on" : "off");
    spdlog::info("  Validation: {}", config.validation_layers ? "on" : "off");
    spdlog::info("  Max agents: {}", config.max_agents);

    auto win_result = init_window(config);
    if (win_result.is_err()) return Result<bool>::err(win_result.error());

    auto vk_result = init_vulkan(config);
    if (vk_result.is_err()) return Result<bool>::err(vk_result.error());

    auto nadir_result = init_nadir(config);
    if (nadir_result.is_err()) return Result<bool>::err(nadir_result.error());

    running_ = true;
    spdlog::info("Engine initialization complete");
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// init_window
// ---------------------------------------------------------------------------

Result<bool> Engine::init_window(const EngineConfig& config) {
#if ODYSSEY_HAS_GLFW
    if (!glfwInit()) {
        return Result<bool>::err("GLFW initialization failed");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // Vulkan — no OpenGL
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(
        static_cast<int>(config.window_width),
        static_cast<int>(config.window_height),
        config.window_title.c_str(),
        nullptr, nullptr);

    if (!window_) {
        glfwTerminate();
        return Result<bool>::err("Failed to create GLFW window");
    }

    // Store a user pointer so callbacks can reach the Engine.
    glfwSetWindowUserPointer(window_, this);

    // Framebuffer resize callback — sets a flag for swapchain recreation.
    glfwSetFramebufferSizeCallback(window_,
        [](GLFWwindow* w, int /*width*/, int /*height*/) {
            // In a full implementation this triggers swapchain recreation.
            spdlog::info("Framebuffer resized");
        });

    spdlog::info("GLFW window created");
#else
    spdlog::warn("GLFW not available — running headless");
    (void)config;
#endif
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// init_vulkan
// ---------------------------------------------------------------------------

Result<bool> Engine::init_vulkan(const EngineConfig& config) {
#if ODYSSEY_HAS_VULKAN && ODYSSEY_HAS_GLFW
    // 1. Instance
    auto inst_cfg = vulkan::compute_instance_config(config.validation_layers);
    auto inst_res = vulkan::create_instance(inst_cfg);
    if (inst_res.is_err()) return Result<bool>::err(inst_res.error());
    impl_->instance = inst_res.value();

    // 2. Surface (through GLFW)
    VkResult vr = glfwCreateWindowSurface(impl_->instance, window_, nullptr,
                                          &impl_->surface);
    if (vr != VK_SUCCESS) {
        return Result<bool>::err("Failed to create Vulkan surface");
    }

    // 3. Device
    vulkan::DeviceConfig dev_cfg{};
    dev_cfg.instance        = impl_->instance;
    dev_cfg.surface         = impl_->surface;
    dev_cfg.preferred_index = config.gpu_index;
    auto dev_res = vulkan::create_device(dev_cfg);
    if (dev_res.is_err()) return Result<bool>::err(dev_res.error());
    impl_->device_ctx = dev_res.value();

    // 4. Swapchain
    vulkan::SwapchainConfig sc_cfg{};
    sc_cfg.desired_width  = config.window_width;
    sc_cfg.desired_height = config.window_height;
    sc_cfg.vsync          = config.vsync;
    auto sc_res = vulkan::create_swapchain(impl_->device_ctx,
                                           impl_->surface, sc_cfg);
    if (sc_res.is_err()) return Result<bool>::err(sc_res.error());
    impl_->swapchain_ctx = sc_res.value();

    // 5. Command pool + buffers
    auto pool_res = vulkan::create_command_pool(
        impl_->device_ctx.device,
        impl_->device_ctx.queue_families.graphics,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    if (pool_res.is_err()) return Result<bool>::err(pool_res.error());
    impl_->command_pool = pool_res.value();

    auto cb_res = vulkan::allocate_command_buffers(
        impl_->device_ctx.device, impl_->command_pool,
        vulkan::MAX_FRAMES_IN_FLIGHT, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    if (cb_res.is_err()) return Result<bool>::err(cb_res.error());
    impl_->command_buffers = cb_res.value();

    // 6. Synchronisation primitives
    impl_->frame_sync.resize(vulkan::MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < vulkan::MAX_FRAMES_IN_FLIGHT; ++i) {
        auto sync_res = vulkan::create_frame_sync(impl_->device_ctx.device);
        if (sync_res.is_err()) return Result<bool>::err(sync_res.error());
        impl_->frame_sync[i] = sync_res.value();
    }

    spdlog::info("Vulkan initialized");
#else
    spdlog::warn("Vulkan headers not available — GPU subsystem skipped");
    (void)config;
#endif
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// init_nadir
// ---------------------------------------------------------------------------

Result<bool> Engine::init_nadir(const EngineConfig& config) {
#if ODYSSEY_HAS_NADIR && ODYSSEY_HAS_VULKAN
    nadir::NadirConfig ncfg{};
    ncfg.behavior_dir      = config.behavior_dir;
    ncfg.lib_dir           = config.lib_dir;
    ncfg.hot_reload_enabled = config.hot_reload;
    ncfg.max_agents        = config.max_agents;
    ncfg.workgroup_size    = NADIR_WORKGROUP_SIZE;

    auto res = impl_->nadir_system.initialize(
        impl_->device_ctx.device, impl_->device_ctx.allocator,
        impl_->descriptor_pool, ncfg);
    if (res.is_err()) return Result<bool>::err(res.error());

    auto load_res = impl_->nadir_system.load_behaviors();
    if (load_res.is_err()) {
        spdlog::warn("Nadir load_behaviors: {}", load_res.error());
        // Non-fatal — hot-reload can pick them up later.
    }

    spdlog::info("Nadir system initialized ({} archetypes)",
                 impl_->nadir_system.archetype_count());
#else
    spdlog::warn("Nadir headers not available — behavior system skipped");
    (void)config;
#endif
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// run — main loop
// ---------------------------------------------------------------------------

void Engine::run() {
#if ODYSSEY_HAS_GLFW
    last_time_ = glfwGetTime();
    spdlog::info("Entering main loop");

    while (running_) {
        if (window_ && glfwWindowShouldClose(window_)) {
            running_ = false;
            break;
        }

        glfwPollEvents();

        double current_time = glfwGetTime();
        float delta = static_cast<float>(current_time - last_time_);
        last_time_ = current_time;

        process_frame(delta);
    }

#   if ODYSSEY_HAS_VULKAN
    if (impl_ && impl_->device_ctx.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(impl_->device_ctx.device);
    }
#   endif

    spdlog::info("Main loop exited");
#else
    spdlog::warn("No GLFW — cannot enter main loop");
#endif
}

// ---------------------------------------------------------------------------
// process_frame
// ---------------------------------------------------------------------------

void Engine::process_frame(float delta_time) {
    total_time_ += delta_time;

#if ODYSSEY_HAS_VULKAN && ODYSSEY_HAS_NADIR
    // --- Hot-reload check ---
    auto changed = impl_->nadir_system.check_hot_reload();
    if (!changed.empty()) {
        spdlog::info("Hot-reloaded {} behavior(s)", changed.size());
    }

    // --- Acquire image ---
    auto& sync = impl_->frame_sync[current_frame_];
    vkWaitForFences(impl_->device_ctx.device, 1, &sync.in_flight, VK_TRUE,
                    UINT64_MAX);
    vkResetFences(impl_->device_ctx.device, 1, &sync.in_flight);

    uint32_t image_index = 0;
    VkResult acquire = vkAcquireNextImageKHR(
        impl_->device_ctx.device, impl_->swapchain_ctx.swapchain, UINT64_MAX,
        sync.image_available, VK_NULL_HANDLE, &image_index);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain recreation would go here.
        return;
    }

    // --- Record command buffer ---
    auto cmd = impl_->command_buffers[current_frame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin_info);

    // Nadir compute dispatches
    impl_->nadir_system.record_dispatches(cmd);

    vkEndCommandBuffer(cmd);

    // --- Submit ---
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkSubmitInfo submit_info{};
    submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount   = 1;
    submit_info.pWaitSemaphores      = &sync.image_available;
    submit_info.pWaitDstStageMask    = &wait_stage;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores    = &sync.render_finished;

    vkQueueSubmit(impl_->device_ctx.compute_queue, 1, &submit_info,
                  sync.in_flight);

    // --- Present ---
    VkPresentInfoKHR present_info{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &sync.render_finished;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &impl_->swapchain_ctx.swapchain;
    present_info.pImageIndices      = &image_index;

    vkQueuePresentKHR(impl_->device_ctx.present_queue, &present_info);

    current_frame_ = (current_frame_ + 1) % vulkan::MAX_FRAMES_IN_FLIGHT;
#else
    (void)delta_time;
#endif
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void Engine::shutdown() {
    spdlog::info("Shutting down engine");
    running_ = false;

    shutdown_nadir();
    shutdown_vulkan();
    shutdown_window();

    impl_.reset();
    spdlog::info("Engine shutdown complete");
}

void Engine::shutdown_nadir() {
#if ODYSSEY_HAS_NADIR
    if (impl_) {
        impl_->nadir_system.shutdown();
        spdlog::info("Nadir system shut down");
    }
#endif
}

void Engine::shutdown_vulkan() {
#if ODYSSEY_HAS_VULKAN
    if (!impl_) return;
    auto dev = impl_->device_ctx.device;
    if (dev == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(dev);

    // Sync objects
    for (auto& s : impl_->frame_sync) {
        vkDestroySemaphore(dev, s.image_available, nullptr);
        vkDestroySemaphore(dev, s.render_finished, nullptr);
        vkDestroyFence(dev, s.in_flight, nullptr);
    }

    // Command pool (frees command buffers implicitly)
    if (impl_->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(dev, impl_->command_pool, nullptr);
    }

    // Swapchain image views + swapchain
    for (auto iv : impl_->swapchain_ctx.image_views) {
        vkDestroyImageView(dev, iv, nullptr);
    }
    if (impl_->swapchain_ctx.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(dev, impl_->swapchain_ctx.swapchain, nullptr);
    }

    // Allocator
    if (impl_->device_ctx.allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(impl_->device_ctx.allocator);
    }

    vkDestroyDevice(dev, nullptr);

    if (impl_->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(impl_->instance, impl_->surface, nullptr);
    }

    if (impl_->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(impl_->instance, nullptr);
    }

    spdlog::info("Vulkan resources destroyed");
#endif
}

void Engine::shutdown_window() {
#if ODYSSEY_HAS_GLFW
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    spdlog::info("GLFW terminated");
#endif
}

} // namespace odyssey
