#include "debug/overlay.h"

#include <spdlog/spdlog.h>

// ImGui integration is optional — allow compilation without ImGui headers
#if __has_include(<imgui.h>) && __has_include(<backends/imgui_impl_glfw.h>)
#define ODYSSEY_HAS_IMGUI 1
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#else
#define ODYSSEY_HAS_IMGUI 0
// Forward-declare GLFW key constants used by handle_key
#ifndef GLFW_KEY_F1
#define GLFW_KEY_F1 290
#define GLFW_KEY_F2 291
#define GLFW_KEY_F3 292
#define GLFW_KEY_F4 293
#define GLFW_KEY_F5 294
#define GLFW_PRESS 1
#endif
#endif

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace odyssey::debug {

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

Result<bool> DebugOverlay::initialize(
    GLFWwindow* window,
    VkInstance instance,
    VkDevice device,
    VkPhysicalDevice physical_device,
    uint32_t graphics_queue_family,
    VkQueue graphics_queue,
    VkRenderPass render_pass,
    uint32_t image_count)
{
#if ODYSSEY_HAS_IMGUI
    // Create a dedicated descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VkResult vk_result = vkCreateDescriptorPool(device, &pool_info, nullptr, &imgui_pool_);
    if (vk_result != VK_SUCCESS) {
        return Result<bool>::err(
            "Failed to create ImGui descriptor pool, VkResult " +
            std::to_string(vk_result));
    }

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Set dark color style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha = config_.overlay_alpha;

    // Initialize platform/renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.QueueFamily = graphics_queue_family;
    init_info.Queue = graphics_queue;
    init_info.DescriptorPool = imgui_pool_;
    init_info.MinImageCount = image_count;
    init_info.ImageCount = image_count;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.RenderPass = render_pass;

    ImGui_ImplVulkan_Init(&init_info);

    initialized_ = true;
    spdlog::info("Debug overlay initialized with ImGui");
    return Result<bool>::ok(true);

#else
    (void)window;
    (void)instance;
    (void)device;
    (void)physical_device;
    (void)graphics_queue_family;
    (void)graphics_queue;
    (void)render_pass;
    (void)image_count;

    initialized_ = false;
    spdlog::warn("Debug overlay: ImGui not available, overlay disabled");
    return Result<bool>::ok(false);
#endif
}

void DebugOverlay::shutdown() {
#if ODYSSEY_HAS_IMGUI
    if (initialized_) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        initialized_ = false;
        spdlog::info("Debug overlay shut down");
    }
#endif

    // Note: imgui_pool_ is destroyed by the caller when the device is destroyed,
    // or we could store the device handle. For safety we just null it here.
    imgui_pool_ = VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Frame begin / end
// ---------------------------------------------------------------------------

void DebugOverlay::begin_frame() {
#if ODYSSEY_HAS_IMGUI
    if (!initialized_ || !overlay_visible_) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Draw enabled panels
    if (config_.show_fps) {
        draw_fps_counter();
    }
    if (config_.show_profiler) {
        draw_profiler_panel();
    }
    if (config_.show_entity_inspector) {
        draw_entity_inspector();
    }
    if (config_.show_nadir_inspector) {
        draw_nadir_inspector();
    }
    if (config_.show_network_stats) {
        draw_network_panel();
    }
#endif
}

void DebugOverlay::end_frame(VkCommandBuffer cmd) {
#if ODYSSEY_HAS_IMGUI
    if (!initialized_ || !overlay_visible_) {
        return;
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
#else
    (void)cmd;
#endif
}

// ---------------------------------------------------------------------------
// Keyboard shortcut handling
// ---------------------------------------------------------------------------

void DebugOverlay::handle_key(int key, int action) {
    if (action != GLFW_PRESS) {
        return;
    }

    switch (key) {
        case GLFW_KEY_F1:
            overlay_visible_ = !overlay_visible_;
            spdlog::debug("Debug overlay {}", overlay_visible_ ? "shown" : "hidden");
            break;
        case GLFW_KEY_F2:
            config_.show_profiler = !config_.show_profiler;
            break;
        case GLFW_KEY_F3:
            config_.show_entity_inspector = !config_.show_entity_inspector;
            break;
        case GLFW_KEY_F4:
            config_.show_nadir_inspector = !config_.show_nadir_inspector;
            break;
        case GLFW_KEY_F5:
            config_.show_network_stats = !config_.show_network_stats;
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Panel drawing (ImGui-dependent)
// ---------------------------------------------------------------------------

void DebugOverlay::draw_fps_counter() {
#if ODYSSEY_HAS_IMGUI
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoFocusOnAppearing
                           | ImGuiWindowFlags_NoNav
                           | ImGuiWindowFlags_NoMove;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos(viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + 10.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);

    if (ImGui::Begin("FPS", nullptr, flags)) {
        float fps = 0.0f;
        float frame_ms = 0.0f;

        if (profiler_) {
            fps = profiler_->current_frame().fps;
            frame_ms = profiler_->current_frame().total_frame_ms;
        }

        ImGui::Text("%.1f FPS (%.2f ms)", fps, frame_ms);
        ImGui::Text("Entities: %u", entity_count_);
    }
    ImGui::End();
#endif
}

void DebugOverlay::draw_profiler_panel() {
#if ODYSSEY_HAS_IMGUI
    if (!profiler_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Profiler", &config_.show_profiler)) {
        const auto& frame = profiler_->current_frame();
        auto stats = profiler_->get_stats();

        // Summary
        ImGui::Text("Frame: %.2f ms | FPS: %.1f", frame.total_frame_ms, frame.fps);
        ImGui::Text("CPU: %.2f ms | GPU: %.2f ms", frame.cpu_time_ms, frame.gpu_time_ms);
        ImGui::Separator();

        // Stats
        if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Avg: %.2f ms | Min: %.2f ms | Max: %.2f ms",
                        stats.avg_frame_ms, stats.min_frame_ms, stats.max_frame_ms);
            ImGui::Text("P99: %.2f ms | Avg FPS: %.1f",
                        stats.p99_frame_ms, stats.avg_fps);
        }

        // Frame time graph
        if (ImGui::CollapsingHeader("Frame Time Graph", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& history = profiler_->history();
            if (!history.empty()) {
                std::vector<float> values;
                values.reserve(history.size());
                for (const auto& h : history) {
                    values.push_back(h.total_frame_ms);
                }
                ImGui::PlotLines("Frame ms", values.data(),
                                 static_cast<int>(values.size()),
                                 0, nullptr, 0.0f, stats.max_frame_ms * 1.2f,
                                 ImVec2(0, 80));
            }
        }

        // GPU timings
        if (!frame.gpu_timings.empty() &&
            ImGui::CollapsingHeader("GPU Sections", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "gpu_cols");
            ImGui::Text("Section"); ImGui::NextColumn();
            ImGui::Text("Time (ms)"); ImGui::NextColumn();
            ImGui::Separator();

            for (const auto& entry : frame.gpu_timings) {
                ImGui::Text("%s", entry.name.c_str()); ImGui::NextColumn();
                ImGui::Text("%.3f", entry.duration_ms); ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }

        // CPU timings
        if (!frame.cpu_timings.empty() &&
            ImGui::CollapsingHeader("CPU Sections")) {
            ImGui::Columns(2, "cpu_cols");
            ImGui::Text("Section"); ImGui::NextColumn();
            ImGui::Text("Time (ms)"); ImGui::NextColumn();
            ImGui::Separator();

            for (const auto& entry : frame.cpu_timings) {
                ImGui::Text("%s", entry.name.c_str()); ImGui::NextColumn();
                ImGui::Text("%.3f", entry.duration_ms); ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
    }
    ImGui::End();
#endif
}

void DebugOverlay::draw_entity_inspector() {
#if ODYSSEY_HAS_IMGUI
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Entity Inspector", &config_.show_entity_inspector)) {
        ImGui::Text("Total Entities: %u", entity_count_);
        ImGui::Separator();

        // List archetypes with entity counts
        if (!nadir_info_.archetypes.empty()) {
            for (const auto& arch : nadir_info_.archetypes) {
                if (ImGui::TreeNode(arch.name.c_str())) {
                    ImGui::Text("Entities: %u", arch.entity_count);
                    ImGui::Text("Shader:   %s", arch.shader_path.c_str());
                    ImGui::TreePop();
                }
            }
        } else {
            ImGui::TextDisabled("No archetype data available");
        }
    }
    ImGui::End();
#endif
}

void DebugOverlay::draw_nadir_inspector() {
#if ODYSSEY_HAS_IMGUI
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Nadir Inspector", &config_.show_nadir_inspector)) {
        ImGui::Text("Archetypes: %zu", nadir_info_.archetypes.size());
        ImGui::Separator();

        if (!nadir_info_.archetypes.empty()) {
            // Table of archetypes
            ImGui::Columns(4, "nadir_cols");
            ImGui::Text("Name"); ImGui::NextColumn();
            ImGui::Text("Entities"); ImGui::NextColumn();
            ImGui::Text("Dispatch (ms)"); ImGui::NextColumn();
            ImGui::Text("Shader"); ImGui::NextColumn();
            ImGui::Separator();

            for (const auto& arch : nadir_info_.archetypes) {
                ImGui::Text("%s", arch.name.c_str()); ImGui::NextColumn();
                ImGui::Text("%u", arch.entity_count); ImGui::NextColumn();
                ImGui::Text("%.3f", arch.dispatch_time_ms); ImGui::NextColumn();
                ImGui::Text("%s", arch.shader_path.c_str()); ImGui::NextColumn();
            }
            ImGui::Columns(1);
        } else {
            ImGui::TextDisabled("No Nadir data available");
        }
    }
    ImGui::End();
#endif
}

void DebugOverlay::draw_network_panel() {
#if ODYSSEY_HAS_IMGUI
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Network", &config_.show_network_stats)) {
        if (net_info_.connected) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
            ImGui::Text("Server: %s", net_info_.server_name.c_str());
            ImGui::Text("RTT: %.1f ms", net_info_.rtt_ms);
            ImGui::Text("Packet Loss: %.1f%%", net_info_.packet_loss * 100.0f);
            ImGui::Text("Players: %u", static_cast<unsigned>(net_info_.player_count));

            // Color-code RTT
            ImVec4 rtt_color;
            if (net_info_.rtt_ms < 50.0f) {
                rtt_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // green
            } else if (net_info_.rtt_ms < 100.0f) {
                rtt_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // yellow
            } else {
                rtt_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // red
            }
            ImGui::TextColored(rtt_color, "Latency: %.0f ms", net_info_.rtt_ms);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Disconnected");
        }
    }
    ImGui::End();
#endif
}

} // namespace odyssey::debug
