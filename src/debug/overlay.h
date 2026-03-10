#pragma once
#include "debug/profiler.h"
#include "core/types.h"
#include "core/result.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct GLFWwindow;

namespace odyssey::debug {

struct OverlayConfig {
    bool show_fps = true;
    bool show_profiler = false;
    bool show_entity_inspector = false;
    bool show_nadir_inspector = false;
    bool show_network_stats = false;
    float overlay_alpha = 0.8f;
};

// Nadir debug info for overlay display
struct NadirDebugInfo {
    struct ArchetypeInfo {
        std::string name;
        uint32_t entity_count;
        float dispatch_time_ms;
        std::string shader_path;
    };
    std::vector<ArchetypeInfo> archetypes;
};

// Network debug info
struct NetworkDebugInfo {
    bool connected = false;
    float rtt_ms = 0.0f;
    float packet_loss = 0.0f;
    uint8_t player_count = 0;
    std::string server_name;
};

class DebugOverlay {
public:
    // Initialize ImGui with Vulkan backend
    Result<bool> initialize(
        GLFWwindow* window,
        VkInstance instance,
        VkDevice device,
        VkPhysicalDevice physical_device,
        uint32_t graphics_queue_family,
        VkQueue graphics_queue,
        VkRenderPass render_pass,
        uint32_t image_count
    );

    void shutdown();

    // Begin/end frame (call between render passes)
    void begin_frame();
    void end_frame(VkCommandBuffer cmd);

    // Update data sources
    void set_profiler(const Profiler* profiler) { profiler_ = profiler; }
    void set_nadir_info(const NadirDebugInfo& info) { nadir_info_ = info; }
    void set_network_info(const NetworkDebugInfo& info) { net_info_ = info; }
    void set_entity_count(uint32_t count) { entity_count_ = count; }

    // Toggle panels
    OverlayConfig& config() { return config_; }

    // Handle keyboard shortcuts (F1=toggle overlay, F2=profiler, etc.)
    void handle_key(int key, int action);

private:
    void draw_fps_counter();
    void draw_profiler_panel();
    void draw_entity_inspector();
    void draw_nadir_inspector();
    void draw_network_panel();

    const Profiler* profiler_ = nullptr;
    NadirDebugInfo nadir_info_;
    NetworkDebugInfo net_info_;
    uint32_t entity_count_ = 0;

    OverlayConfig config_;
    bool overlay_visible_ = true;

    // ImGui Vulkan resources
    VkDescriptorPool imgui_pool_ = VK_NULL_HANDLE;
    bool initialized_ = false;
};

} // namespace odyssey::debug
