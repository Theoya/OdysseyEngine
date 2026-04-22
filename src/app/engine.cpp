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
#   include "vulkan/bindless_texture_registry.h"
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
#include "scene/scene_loader.h"
#include "assets/lighting_profile_loader.h"

#include <optional>

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

    // Bindless texture registry — set=0, lifetime matches the Vulkan device.
    // Initialized after the device (needs DeviceContext + command pool) and
    // before the renderer (renderer.attach_bindless_registry must precede
    // renderer.initialize so the pipeline layout includes set=0).
    vulkan::BindlessTextureRegistry   bindless_registry;
#endif

#if ODYSSEY_HAS_NADIR
    nadir::NadirSystem                nadir_system;
#endif

    Camera                            camera;
    InputManager                      input;
    scene::EntityManager              entity_mgr;
    EngineConfig                      config;
    std::unique_ptr<Game>             game;

    // Lighting profile loaded at scene-open time.
    // Absent when the scene has no lighting_profile attribute or when
    // loading fails (error is logged and engine continues unaffected).
    std::optional<assets::LightingProfileData> active_lighting_profile;
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
        config.fullscreen    = win.attribute("fullscreen").as_bool(config.fullscreen);
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

    // Scene settings
    if (auto scene = root.child("scene")) {
        config.scene_path = scene.attribute("path").as_string(config.scene_path.string().c_str());
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
// set_mode — Phase 2 mode gating entry point.
//
// Effects, by mode:
//   Edit      — Nadir dispatch skipped, scripts + physics skipped (the game
//               honors ctx.mode). Camera input still feeds so the user can
//               navigate the scene freely.
//   Play      — Full simulation (original behavior).
//   Simulate  — Nadir + physics run, scripts paused. Used for tuning AI.
//
// Safe to call from any thread the main loop serialises with — the Editor
// calls it synchronously when the user clicks a toolbar button; the Engine
// applies the change on the next process_frame().
// ---------------------------------------------------------------------------
void Engine::set_mode(Mode m) {
    if (mode_ == m) return;
    mode_ = m;
    spdlog::info("Engine mode -> {}", mode_label(m));
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
        ctx.mode = mode_;

        auto game_res = impl_->game->on_init(ctx);
        if (game_res.is_err()) {
            spdlog::warn("Game init failed: {}", game_res.error());
        } else {
            spdlog::info("Game initialized: {} renderables",
                         impl_->game->get_renderables().size());
        }
    }

    // --- Resolve lighting profile from scene attributes ---
    // If the scene file was specified, load it now (purely for attribute
    // inspection — entity population was already handled by the Game).
    // The scene_loader call here is a cheap re-parse; SceneData is
    // pure data with no GPU dependency.
    if (!config.scene_path.empty()) {
        auto scene_res = scene::load_scene_file(config.scene_path);
        if (scene_res.is_ok()) {
            const auto& scene_data = scene_res.value();
            // Look for lighting_profile="<name>" in unknown_scene_attributes.
            for (const auto& [key, value] : scene_data.unknown_scene_attributes) {
                if (key == "lighting_profile" && !value.empty()) {
                    spdlog::info("Engine: resolving lighting profile '{}'", value);
                    const auto scene_dir =
                        std::filesystem::path(config.scene_path).parent_path();
                    const auto candidates =
                        assets::resolve_lighting_profile_path(value, scene_dir);
                    bool loaded = false;
                    for (const auto& candidate : candidates) {
                        if (!std::filesystem::exists(candidate)) continue;
                        auto prof_res = assets::load_lighting_profile_file(candidate);
                        if (prof_res.is_ok()) {
                            impl_->active_lighting_profile = std::move(prof_res.value());
                            spdlog::info("Engine: lighting profile '{}' loaded from '{}'",
                                         value, candidate.string());
                            loaded = true;
                            break;
                        } else {
                            spdlog::warn("Engine: lighting profile load failed for '{}': "
                                         "error code {}",
                                         candidate.string(),
                                         static_cast<uint32_t>(prof_res.error()));
                        }
                    }
                    if (!loaded) {
                        spdlog::warn("Engine: no valid lighting profile found for name '{}' "
                                     "— rendering with engine defaults", value);
                    }
                    break;
                }
            }
        }
        // Scene load failure here is non-fatal: the Game already loaded the
        // scene for entity population; this second pass is only for metadata.
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

    GLFWmonitor* monitor = nullptr;
    int win_w = static_cast<int>(config.window_width);
    int win_h = static_cast<int>(config.window_height);

    if (config.fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        win_w = mode->width;
        win_h = mode->height;
        fullscreen_ = true;
    } else {
        windowed_w_ = win_w;
        windowed_h_ = win_h;
    }

    window_ = glfwCreateWindow(win_w, win_h,
        config.window_title.c_str(),
        monitor, nullptr);

    if (!window_) {
        glfwTerminate();
        return Result<bool>::err("Failed to create GLFW window");
    }

    // Store a user pointer so callbacks can reach the Engine.
    glfwSetWindowUserPointer(window_, this);

    // Framebuffer resize callback
    glfwSetFramebufferSizeCallback(window_,
        [](GLFWwindow* w, int /*width*/, int /*height*/) {
            auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(w));
            if (engine) engine->framebuffer_resized_ = true;
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
    if (dev_res.is_err()) return Result<bool>::err(vulkan::device_create_err_to_string(dev_res.error()));
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

    // 9. Bindless texture registry — must come after device + command pool
    //    are ready, and before renderer.initialize() which bakes set=0 into
    //    the pipeline layout.  initialize() uploads the 1×1 magenta sentinel
    //    to slot 0 so any unresolved material_index=0 renders magenta rather
    //    than sampling undefined memory.
    {
        auto br_res = impl_->bindless_registry.initialize(
            impl_->device_ctx, impl_->command_pool);
        if (br_res.is_err()) {
            return Result<bool>::err(
                "BindlessTextureRegistry init failed: " + br_res.error());
        }
        spdlog::info("BindlessTextureRegistry ready ({} slots, sentinel uploaded)",
                     impl_->bindless_registry.capacity());
    }

    // 10. Renderer — attach bindless registry BEFORE initialize() so the
    //     pipeline layout includes set=0.  If attach is omitted the renderer
    //     falls back to push-constant-color only (no bindless sample).
    {
        impl_->renderer.attach_bindless_registry(&impl_->bindless_registry);
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

    // Set transfer context so upload/readback works
    impl_->nadir_system.set_transfer_context(impl_->device_ctx, impl_->command_pool);

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

    // Fullscreen toggle (F11, edge-triggered)
    bool f11_down = impl_->input.is_key_down(GLFW_KEY_F11);
    if (f11_down && !f11_was_pressed_) {
        toggle_fullscreen();
    }
    f11_was_pressed_ = f11_down;

#if ODYSSEY_HAS_VULKAN
    // --- Wait for previous frame's GPU work to complete ---
    auto& sync = impl_->frame_sync[current_frame_];
    vkWaitForFences(impl_->device_ctx.device, 1, &sync.in_flight, VK_TRUE,
                    UINT64_MAX);

    // --- Hot-reload check ---
#if ODYSSEY_HAS_NADIR
    auto changed = impl_->nadir_system.check_hot_reload();
    if (!changed.empty()) {
        spdlog::info("Hot-reloaded {} behavior(s)", changed.size());
    }
#endif

    // --- Game tick (GPU outputs now safe to readback) ---
    // Phase 2 mode gating: in Edit mode we skip the game tick entirely —
    // this naturally pauses scripts AND physics since both are driven by
    // the game. In Simulate we still tick so the game can step physics,
    // but the game is expected to honor ctx.mode and skip script logic.
    if (impl_->game && mode_ != Mode::Edit) {
        GameContext ctx{};
        ctx.delta_time = mode_runs_physics(mode_) ? delta_time : 0.0f;
        ctx.total_time = total_time_;
        ctx.camera = &impl_->camera;
        ctx.input = &impl_->input;
        ctx.entity_mgr = &impl_->entity_mgr;
#if ODYSSEY_HAS_NADIR
        ctx.nadir_sys = &impl_->nadir_system;
#endif
        ctx.window = window_;
        ctx.scene_path = impl_->config.scene_path;
        ctx.mode = mode_;
        impl_->game->on_tick(ctx);
    }

    // --- Phase 9: Physics step (fixed-dt accumulator) ---
    // Tick order per Phase 9 spec:
    //   1. InputManager poll (completed above)
    //   2. Script::pre_physics tick (via game->on_tick, above)
    //   3. PhysicsWorld::step (fixed-substep loop, here)
    //   4. Script::post_physics tick (todo: game could have a post_tick callback)
    //   5. EntityManager::compose_world_transforms (happens in game->on_tick)
    //   6. NadirSystem dispatch (below)
    //   7. Renderer::render_frame (below)
    //   8. AudioMixer::mix (todo: audio subsystem)
    if (mode_runs_physics(mode_)) {
        // Fixed-dt accumulator: 60 Hz = 1.0/60.0 canonical dt
        constexpr float kFixedDt = 1.0f / 60.0f;
        constexpr uint32_t kMaxSubsteps = 5;
        physics_accumulator_ += delta_time;
        uint32_t substeps = 0;

        while (physics_accumulator_ >= kFixedDt && substeps < kMaxSubsteps) {
            physics_world_.step(kFixedDt);
            physics_accumulator_ -= kFixedDt;
            ++substeps;
        }

        // After physics, writeback Rigidbody positions to entity local transforms
        // (This is a placeholder — the actual implementation depends on
        // component bindings added in Commit O)

        // Compose world transforms (topological sort on parent hierarchy)
        // The game's on_tick may have already called this; doing it again is safe
        // (idempotent) but may be redundant.
        if (impl_->entity_mgr.compose_world_transforms().is_err()) {
            spdlog::error("Failed to compose world transforms after physics step");
        }
    }

    // --- Acquire image ---
    uint32_t image_index = 0;
    VkResult acquire = vkAcquireNextImageKHR(
        impl_->device_ctx.device, impl_->swapchain_ctx.swapchain, UINT64_MAX,
        sync.image_available, VK_NULL_HANDLE, &image_index);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }

    // Reset fence only after successful acquire (avoids deadlock on resize)
    vkResetFences(impl_->device_ctx.device, 1, &sync.in_flight);

    // --- Record command buffer ---
    auto cmd = impl_->command_buffers[current_frame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin_info);

    // Nadir compute dispatches — gated by Phase 2 execution mode.
    // Edit mode: skip dispatch entirely. Play/Simulate: run as normal.
#if ODYSSEY_HAS_NADIR
    if (mode_runs_nadir(mode_)) {
        impl_->nadir_system.record_dispatches(cmd);
    }
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

            // Overlay the scene's lighting profile on top of the HUD-adjusted
            // base CRT params.  profile_to_crt_params is pure — it reads
            // base_crt (which already incorporates HUDParams boosts) and
            // returns a new CRTParams with the profile's photographic intent
            // applied (exposure scale, vignette replacement, grain→flicker).
            if (impl_->active_lighting_profile.has_value()) {
                crt = assets::profile_to_crt_params(
                    crt, impl_->active_lighting_profile.value());
                eva = assets::profile_to_eva_params(
                    eva, impl_->active_lighting_profile.value());
            }

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

    VkResult present_result = vkQueuePresentKHR(impl_->device_ctx.present_queue, &present_info);

    if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
        present_result == VK_SUBOPTIMAL_KHR ||
        framebuffer_resized_) {
        recreate_swapchain();
    }

    current_frame_ = (current_frame_ + 1) % vulkan::MAX_FRAMES_IN_FLIGHT;

    // --- Update window title from game HUD ---
    if (impl_->game) {
        auto hud = impl_->game->get_hud_params();
        if (!hud.window_title.empty()) {
            glfwSetWindowTitle(window_, hud.window_title.c_str());
        }
    }
#else
    // Non-Vulkan: game tick without GPU (still mode-gated).
    if (impl_->game && mode_ != Mode::Edit) {
        GameContext ctx{};
        ctx.delta_time = mode_runs_physics(mode_) ? delta_time : 0.0f;
        ctx.total_time = total_time_;
        ctx.camera = &impl_->camera;
        ctx.input = &impl_->input;
        ctx.entity_mgr = &impl_->entity_mgr;
        ctx.window = window_;
        ctx.scene_path = impl_->config.scene_path;
        ctx.mode = mode_;
        impl_->game->on_tick(ctx);
    }
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
        if (entity.scale.x <= 0.f) continue;

        mat4 model = glm::translate(mat4(1.0f), entity.position);
        model *= glm::mat4_cast(entity.rotation);
        model = glm::scale(model, entity.scale);

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
// recreate_swapchain
// ---------------------------------------------------------------------------

void Engine::recreate_swapchain() {
#if ODYSSEY_HAS_VULKAN && ODYSSEY_HAS_GLFW
    // Wait until framebuffer is non-zero (handles minimization)
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(impl_->device_ctx.device);

    // Save old swapchain handle for driver resource recycling
    auto old_swapchain = impl_->swapchain_ctx.swapchain;

    // Destroy old swapchain image views
    for (auto iv : impl_->swapchain_ctx.image_views) {
        vkDestroyImageView(impl_->device_ctx.device, iv, nullptr);
    }
    impl_->swapchain_ctx.image_views.clear();
    impl_->swapchain_ctx.images.clear();

    // Recreate swapchain (passes old handle so the driver can recycle resources)
    auto sc_cfg = vulkan::compute_swapchain_config(
        impl_->device_ctx.physical_device, impl_->surface,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height),
        impl_->config.vsync);
    auto sc_res = vulkan::create_swapchain(impl_->device_ctx,
                                           impl_->surface, sc_cfg,
                                           old_swapchain);

    // Destroy retired old swapchain
    vkDestroySwapchainKHR(impl_->device_ctx.device, old_swapchain, nullptr);

    if (sc_res.is_err()) {
        spdlog::error("Failed to recreate swapchain: {}", sc_res.error());
        return;
    }
    impl_->swapchain_ctx = sc_res.value();

    // Recreate renderer depth buffer + framebuffers
    auto ren_res = impl_->renderer.recreate_for_resize(
        impl_->swapchain_ctx.extent, impl_->swapchain_ctx.image_views);
    if (ren_res.is_err()) {
        spdlog::error("Renderer resize failed: {}", ren_res.error());
    }

    // Recreate post-processor offscreen target + framebuffers
    if (impl_->has_postprocessor) {
        auto pp_res = impl_->postprocessor.recreate_for_resize(
            impl_->swapchain_ctx.extent, impl_->swapchain_ctx.image_views);
        if (pp_res.is_err()) {
            spdlog::error("PostProcessor resize failed: {}", pp_res.error());
        }
    }

    framebuffer_resized_ = false;
    spdlog::info("Swapchain recreated: {}x{}",
                 impl_->swapchain_ctx.extent.width,
                 impl_->swapchain_ctx.extent.height);
#endif
}

// ---------------------------------------------------------------------------
// toggle_fullscreen
// ---------------------------------------------------------------------------

void Engine::toggle_fullscreen() {
#if ODYSSEY_HAS_GLFW
    if (!window_) return;

    fullscreen_ = !fullscreen_;

    if (fullscreen_) {
        // Save windowed geometry
        glfwGetWindowPos(window_, &windowed_x_, &windowed_y_);
        glfwGetWindowSize(window_, &windowed_w_, &windowed_h_);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window_, monitor, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window_, nullptr,
                             windowed_x_, windowed_y_,
                             windowed_w_, windowed_h_, 0);
    }

    framebuffer_resized_ = true;
    spdlog::info("Fullscreen: {}", fullscreen_ ? "on" : "off");
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

    // Renderer (must shut down before bindless_registry — it holds a
    // non-owning pointer to the registry's descriptor set).
    impl_->renderer.shutdown();

    // Bindless texture registry — shut down after the renderer (which used
    // the descriptor set) but before the command pool + VMA allocator (which
    // it owns internally via its staging upload path).
    impl_->bindless_registry.shutdown();

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
