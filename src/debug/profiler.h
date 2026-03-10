#pragma once
#include "core/result.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace odyssey::debug {

// Pure: timing data for a single frame
struct FrameProfile {
    float total_frame_ms = 0.0f;
    float cpu_time_ms = 0.0f;
    float gpu_time_ms = 0.0f;

    struct TimingEntry {
        std::string name;
        float duration_ms = 0.0f;
        float start_ms = 0.0f;
    };

    std::vector<TimingEntry> gpu_timings;  // per-archetype dispatch times
    std::vector<TimingEntry> cpu_timings;  // per-section CPU times

    uint32_t entity_count = 0;
    uint32_t archetype_count = 0;
    uint32_t draw_calls = 0;
    float fps = 0.0f;
};

// Pure: compute aggregate stats from frame history
struct ProfileStats {
    float avg_frame_ms = 0.0f;
    float min_frame_ms = 0.0f;
    float max_frame_ms = 0.0f;
    float avg_fps = 0.0f;
    float p99_frame_ms = 0.0f;

    float avg_gpu_ms = 0.0f;
    float avg_cpu_ms = 0.0f;
};

ProfileStats compute_profile_stats(const std::vector<FrameProfile>& history);

// Format profile as text (for CLI output)
std::string format_frame_profile(const FrameProfile& profile);
std::string format_profile_stats(const ProfileStats& stats);

class Profiler {
public:
    // Initialize with Vulkan device (for GPU timestamp queries)
    Result<bool> initialize(VkDevice device, VkPhysicalDevice physical_device,
                            uint32_t max_timestamps = 64);
    void shutdown(VkDevice device);

    // Begin/end frame profiling
    void begin_frame();
    void end_frame();

    // CPU timing
    void begin_cpu_section(const std::string& name);
    void end_cpu_section(const std::string& name);

    // GPU timing (record timestamp queries into command buffer)
    void begin_gpu_section(VkCommandBuffer cmd, const std::string& name);
    void end_gpu_section(VkCommandBuffer cmd, const std::string& name);

    // Collect GPU timestamps from previous frame
    void collect_gpu_timestamps(VkDevice device);

    // Get current frame profile
    const FrameProfile& current_frame() const { return current_frame_; }

    // Get frame history
    const std::vector<FrameProfile>& history() const { return history_; }

    // Get aggregate stats
    ProfileStats get_stats() const;

    // Set max history size
    void set_history_size(size_t max_frames) { max_history_ = max_frames; }

private:
    FrameProfile current_frame_;
    std::vector<FrameProfile> history_;
    size_t max_history_ = 300;  // ~5 seconds at 60fps

    // CPU timing
    using Clock = std::chrono::high_resolution_clock;
    std::unordered_map<std::string, Clock::time_point> cpu_section_starts_;
    Clock::time_point frame_start_;

    // GPU timing
    VkQueryPool query_pool_ = VK_NULL_HANDLE;
    uint32_t max_timestamps_ = 64;
    uint32_t next_query_ = 0;
    float timestamp_period_ = 1.0f; // nanoseconds per tick

    struct GpuSection {
        std::string name;
        uint32_t start_query;
        uint32_t end_query;
    };
    std::vector<GpuSection> gpu_sections_;
    std::vector<GpuSection> prev_gpu_sections_; // for readback
};

} // namespace odyssey::debug
