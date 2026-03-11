#include "app/engine.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <chrono>

// ---------------------------------------------------------------------------
// Vulkan / Nadir / Rendering headers
// ---------------------------------------------------------------------------

#if __has_include("vulkan/instance.h")
#   include "vulkan/instance.h"
#   include "vulkan/device.h"
#   include "vulkan/swapchain.h"
#   include "vulkan/buffer.h"
#   include "vulkan/command.h"
#   include "vulkan/compute_pipeline.h"
#   include "vulkan/renderer.h"
#   include "vulkan/postprocess.h"
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

#include "app/camera.h"
#include "app/input.h"
#include "app/game.h"
#include "scene/entity_manager.h"

#include <glm/gtc/matrix_transform.hpp>

namespace odyssey {

// ---------------------------------------------------------------------------
// Pimpl — holds all runtime state.
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

    vulkan::Renderer                  renderer;
    vulkan::PostProcessor             postprocessor;
    bool                              has_postprocessor = false;
#endif

#if ODYSSEY_HAS_NADIR
    nadir::NadirSystem                nadir_system;
#endif

    Camera                            camera;
    InputManager                      input;
    scene::EntityManager              entity_mgr;
    EngineConfig                      config;
    std::unique_ptr<Game>             game;
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

Result<bool> Engine::initialize(const EngineConfig& config,
                                std::unique_ptr<Game> game) {
    impl_ = std::make_unique<Impl>();
    impl_->config = config;
    impl_->game = std::move(game);

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

    // Initialize input manager
#if ODYSSEY_HAS_GLFW
    if (window_) {
        impl_->input.initialize(window_);
    }
#endif

    // Initialize game (if provided)
    if (impl_->game) {
        GameContext ctx{};
        ctx.camera = &impl_->camera;
        ctx.input = &impl_->input;
        ctx.entity_mgr = &impl_->entity_mgr;
#if ODYSSEY_HAS_NADIR
        ctx.nadir_sys = &impl_->nadir_system;
#endif
        ctx.window = window_;
        ctx.scene_path = config.scene_path;

        auto game_res = impl_->game->on_init(ctx);
        if (game_res.is_err()) {
            spdlog::warn("Game init failed: {}", game_res.error());
        } else {
            spdlog::info("Game initialized: {} renderables",
                         impl_->game->get_renderables().size());
        }
    }

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

    // Framebuffer resize callback
    glfwSetFramebufferSizeCallback(window_,
        [](GLFWwindow* /*w*/, int /*width*/, int /*height*/) {
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

    // 3. Device — select physical device then create logical device
    auto dev_cfg = vulkan::select_physical_device(
        impl_->instance, impl_->surface, config.gpu_index);
    auto dev_res = vulkan::create_device(dev_cfg, impl_->instance);
    if (dev_res.is_err()) return Result<bool>::err(dev_res.error());
    impl_->device_ctx = dev_res.value();

    // 4. Swapchain — compute config from surface capabilities
    auto sc_cfg = vulkan::compute_swapchain_config(
        impl_->device_ctx.physical_device, impl_->surface,
        config.window_width, config.window_height, config.vsync);
    auto sc_res = vulkan::create_swapchain(impl_->device_ctx,
                                           impl_->surface, sc_cfg);
    if (sc_res.is_err()) return Result<bool>::err(sc_res.error());
    impl_->swapchain_ctx = sc_res.value();

    // 5. Command pool + buffers
    auto pool_res = vulkan::create_command_pool(
        impl_->device_ctx.device,
        impl_->device_ctx.queue_families.graphics.value(),
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

    // 7. Descriptor pool for Nadir
    {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6 * 64 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * 64 },
        };
        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.maxSets = 64;
        pool_info.poolSizeCount = 2;
        pool_info.pPoolSizes = pool_sizes;
        VkResult vr2 = vkCreateDescriptorPool(impl_->device_ctx.device,
                                               &pool_info, nullptr,
                                               &impl_->descriptor_pool);
        if (vr2 != VK_SUCCESS) {
            return Result<bool>::err("Failed to create descriptor pool");
        }
    }

    // 8. PostProcessor — creates offscreen render target, scene render pass
    {
        auto pp_res = impl_->postprocessor.initialize(
            impl_->device_ctx,
            impl_->swapchain_ctx.extent,
            impl_->swapchain_ctx.format,
            impl_->command_pool,
            config.shader_dir);
        if (pp_res.is_err()) {
            spdlog::warn("PostProcessor init failed: {} — falling back to direct rendering",
                         pp_res.error());
            impl_->has_postprocessor = false;
        } else {
            // Provide swapchain image views so post framebuffers can be created
            auto sv_res = impl_->postprocessor.set_swapchain_views(
                impl_->swapchain_ctx.image_views, impl_->swapchain_ctx.extent);
            if (sv_res.is_err()) {
                spdlog::warn("PostProcessor swapchain views failed: {}", sv_res.error());
                impl_->postprocessor.shutdown();
                impl_->has_postprocessor = false;
            } else {
                impl_->has_postprocessor = true;
                spdlog::info("PostProcessor ready (CRT + EVA HUD)");
            }
        }
    }

    // 9. Renderer — creates graphics pipeline, primitive meshes
    {
        auto ren_res = impl_->renderer.initialize(
            impl_->device_ctx, impl_->swapchain_ctx, impl_->command_pool);
        if (ren_res.is_err()) return Result<bool>::err(ren_res.error());
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

        // Cap delta to avoid physics explosions on debugger pauses
        if (delta > 0.1f) delta = 0.1f;

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

    // --- Input ---
    impl_->input.update();

    // --- Camera ---
    if (impl_->input.is_cursor_captured()) {
        vec2 md = impl_->input.mouse_delta();
        impl_->camera.update(
            delta_time,
            impl_->input.is_key_down(GLFW_KEY_W),
            impl_->input.is_key_down(GLFW_KEY_S),
            impl_->input.is_key_down(GLFW_KEY_A),
            impl_->input.is_key_down(GLFW_KEY_D),
            impl_->input.is_key_down(GLFW_KEY_SPACE),
            impl_->input.is_key_down(GLFW_KEY_LEFT_SHIFT),
            md.x, md.y);
    }

    // --- Game tick ---
    if (impl_->game) {
        GameContext ctx{};
        ctx.delta_time = delta_time;
        ctx.total_time = total_time_;
        ctx.camera = &impl_->camera;
        ctx.input = &impl_->input;
        ctx.entity_mgr = &impl_->entity_mgr;
#if ODYSSEY_HAS_NADIR
        ctx.nadir_sys = &impl_->nadir_system;
#endif
        ctx.window = window_;
        ctx.scene_path = impl_->config.scene_path;
        impl_->game->on_tick(ctx);
    }

#if ODYSSEY_HAS_VULKAN
    // --- Hot-reload check ---
#if ODYSSEY_HAS_NADIR
    auto changed = impl_->nadir_system.check_hot_reload();
    if (!changed.empty()) {
        spdlog::info("Hot-reloaded {} behavior(s)", changed.size());
    }
#endif

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
        return;
    }

    // --- Record command buffer ---
    auto cmd = impl_->command_buffers[current_frame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin_info);

    // Nadir compute dispatches
#if ODYSSEY_HAS_NADIR
    impl_->nadir_system.record_dispatches(cmd);
#endif

    // --- Build view-projection matrix ---
    float aspect = static_cast<float>(impl_->swapchain_ctx.extent.width)
                 / static_cast<float>(impl_->swapchain_ctx.extent.height);
    mat4 vp = impl_->camera.vp_matrix(aspect);

    // --- Render scene ---
    if (impl_->has_postprocessor) {
        // Render into PostProcessor's offscreen target
        auto bf_res = impl_->renderer.begin_frame_offscreen(
            impl_->postprocessor.scene_render_pass(),
            impl_->postprocessor.scene_framebuffer(),
            impl_->swapchain_ctx.extent,
            cmd);
        if (bf_res.is_ok()) {
            render_entities(vp);
            impl_->renderer.end_frame(cmd);

            // Apply CRT + EVA HUD post-processing → swapchain
            // Get HUD params from game (or use defaults)
            HUDParams hud = impl_->game ? impl_->game->get_hud_params() : HUDParams{};

            vulkan::EvaHUDParams eva{};
            eva.time = total_time_;
            eva.health_pct = hud.health_pct;
            eva.alert_level = hud.alert_level;
            eva.sync_ratio = hud.sync_ratio;

            vulkan::CRTParams crt{};
            crt.time = total_time_;
            crt.brightness = 1.2f + hud.brightness_boost;
            crt.chromatic_aberration = 1.0f + hud.chromatic_boost;
            crt.flicker_amount = 0.2f + hud.flicker_boost;
            crt.curvature = 2.0f + hud.curvature_boost;
            crt.vignette_strength = 0.8f + hud.vignette_boost;

            impl_->postprocessor.apply(cmd, image_index, crt, eva);
        }
    } else {
        // Direct render to swapchain (no post-processing)
        auto bf_res = impl_->renderer.begin_frame(image_index, cmd);
        if (bf_res.is_ok()) {
            render_entities(vp);
            impl_->renderer.end_frame(cmd);
        }
    }

    vkEndCommandBuffer(cmd);

    // --- Submit ---
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{};
    submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount   = 1;
    submit_info.pWaitSemaphores      = &sync.image_available;
    submit_info.pWaitDstStageMask    = &wait_stage;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores    = &sync.render_finished;

    vkQueueSubmit(impl_->device_ctx.graphics_queue, 1, &submit_info,
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

    // --- Update window title from game HUD ---
    if (impl_->game) {
        auto hud = impl_->game->get_hud_params();
        if (!hud.window_title.empty()) {
            glfwSetWindowTitle(window_, hud.window_title.c_str());
        }
    }
#else
    (void)delta_time;
#endif
}

// ---------------------------------------------------------------------------
// render_entities — draw all scene renderables
// ---------------------------------------------------------------------------

void Engine::render_entities(const mat4& vp) {
#if ODYSSEY_HAS_VULKAN
    if (!impl_->game) return;

    const auto& renderables = impl_->game->get_renderables();

    for (const auto& entity : renderables) {
        if (entity.scale <= 0.f) continue;

        mat4 model = glm::translate(mat4(1.0f), entity.position);
        model = glm::scale(model, vec3(entity.scale));

        mat4 mvp = vp * model;

        auto mesh_type = static_cast<vulkan::PrimitiveType>(entity.mesh_type);
        impl_->renderer.draw(mvp, entity.color, mesh_type);
    }

    // Crosshair
    vec3 crosshair_pos = impl_->camera.position()
        + impl_->camera.front() * 3.0f;
    mat4 crosshair_model = glm::translate(mat4(1.0f), crosshair_pos);
    crosshair_model = glm::scale(crosshair_model, vec3(0.02f));
    mat4 crosshair_mvp = vp * crosshair_model;
    impl_->renderer.draw(crosshair_mvp, {1.0f, 1.0f, 1.0f, 1.0f},
                         vulkan::PrimitiveType::SPHERE);
#else
    (void)vp;
#endif
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void Engine::shutdown() {
    spdlog::info("Shutting down engine");
    running_ = false;

    if (impl_ && impl_->game) {
        impl_->game->on_shutdown();
    }

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

    // Renderer
    impl_->renderer.shutdown();

    // PostProcessor
    if (impl_->has_postprocessor) {
        impl_->postprocessor.shutdown();
    }

    // Sync objects
    for (auto& s : impl_->frame_sync) {
        vkDestroySemaphore(dev, s.image_available, nullptr);
        vkDestroySemaphore(dev, s.render_finished, nullptr);
        vkDestroyFence(dev, s.in_flight, nullptr);
    }

    // Descriptor pool
    if (impl_->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, impl_->descriptor_pool, nullptr);
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
