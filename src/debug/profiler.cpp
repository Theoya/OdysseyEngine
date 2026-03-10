#include "debug/profiler.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace odyssey::debug {

// ---------------------------------------------------------------------------
// Pure: compute aggregate stats from frame history
// ---------------------------------------------------------------------------

ProfileStats compute_profile_stats(const std::vector<FrameProfile>& history) {
    ProfileStats stats;

    if (history.empty()) {
        return stats;
    }

    // Collect frame times for sorting (needed for p99)
    std::vector<float> frame_times;
    frame_times.reserve(history.size());

    float total_frame = 0.0f;
    float total_gpu = 0.0f;
    float total_cpu = 0.0f;
    float total_fps = 0.0f;
    float min_frame = std::numeric_limits<float>::max();
    float max_frame = 0.0f;

    for (const auto& frame : history) {
        total_frame += frame.total_frame_ms;
        total_gpu += frame.gpu_time_ms;
        total_cpu += frame.cpu_time_ms;
        total_fps += frame.fps;

        min_frame = std::min(min_frame, frame.total_frame_ms);
        max_frame = std::max(max_frame, frame.total_frame_ms);

        frame_times.push_back(frame.total_frame_ms);
    }

    const auto n = static_cast<float>(history.size());
    stats.avg_frame_ms = total_frame / n;
    stats.min_frame_ms = min_frame;
    stats.max_frame_ms = max_frame;
    stats.avg_fps = total_fps / n;
    stats.avg_gpu_ms = total_gpu / n;
    stats.avg_cpu_ms = total_cpu / n;

    // p99: sort and pick the 99th percentile value
    std::sort(frame_times.begin(), frame_times.end());
    size_t p99_index = static_cast<size_t>(
        std::ceil(0.99f * static_cast<float>(frame_times.size())) - 1.0f);
    p99_index = std::min(p99_index, frame_times.size() - 1);
    stats.p99_frame_ms = frame_times[p99_index];

    return stats;
}

// ---------------------------------------------------------------------------
// Pure: format frame profile as human-readable text
// ---------------------------------------------------------------------------

std::string format_frame_profile(const FrameProfile& profile) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "--- Frame Profile ---\n";
    ss << "  FPS:        " << profile.fps << "\n";
    ss << "  Frame:      " << profile.total_frame_ms << " ms\n";
    ss << "  CPU:        " << profile.cpu_time_ms << " ms\n";
    ss << "  GPU:        " << profile.gpu_time_ms << " ms\n";
    ss << "  Entities:   " << profile.entity_count << "\n";
    ss << "  Archetypes: " << profile.archetype_count << "\n";
    ss << "  Draw calls: " << profile.draw_calls << "\n";

    if (!profile.gpu_timings.empty()) {
        ss << "  GPU Sections:\n";
        for (const auto& entry : profile.gpu_timings) {
            ss << "    " << std::setw(24) << std::left << entry.name
               << std::setw(8) << std::right << entry.duration_ms << " ms\n";
        }
    }

    if (!profile.cpu_timings.empty()) {
        ss << "  CPU Sections:\n";
        for (const auto& entry : profile.cpu_timings) {
            ss << "    " << std::setw(24) << std::left << entry.name
               << std::setw(8) << std::right << entry.duration_ms << " ms\n";
        }
    }

    return ss.str();
}

std::string format_profile_stats(const ProfileStats& stats) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "--- Profile Statistics ---\n";
    ss << "  Avg Frame: " << stats.avg_frame_ms << " ms\n";
    ss << "  Min Frame: " << stats.min_frame_ms << " ms\n";
    ss << "  Max Frame: " << stats.max_frame_ms << " ms\n";
    ss << "  P99 Frame: " << stats.p99_frame_ms << " ms\n";
    ss << "  Avg FPS:   " << stats.avg_fps << "\n";
    ss << "  Avg GPU:   " << stats.avg_gpu_ms << " ms\n";
    ss << "  Avg CPU:   " << stats.avg_cpu_ms << " ms\n";

    return ss.str();
}

// ---------------------------------------------------------------------------
// Profiler — initialization / shutdown
// ---------------------------------------------------------------------------

Result<bool> Profiler::initialize(VkDevice device, VkPhysicalDevice physical_device,
                                  uint32_t max_timestamps) {
    max_timestamps_ = max_timestamps;

    // Query the timestamp period from the physical device properties
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    timestamp_period_ = props.limits.timestampPeriod; // nanoseconds per tick

    if (timestamp_period_ == 0.0f) {
        spdlog::warn("Profiler: timestampPeriod is 0, GPU timing unavailable");
        timestamp_period_ = 1.0f;
    }

    // Check that the graphics queue family supports timestamps
    // (timestampValidBits > 0 is required)

    // Create query pool for timestamp queries
    VkQueryPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    pool_info.queryCount = max_timestamps_;

    VkResult vk_result = vkCreateQueryPool(device, &pool_info, nullptr, &query_pool_);
    if (vk_result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateQueryPool failed with VkResult " + std::to_string(vk_result));
    }

    spdlog::info("Profiler initialized (max_timestamps={}, period={:.2f} ns)",
                 max_timestamps_, timestamp_period_);
    return Result<bool>::ok(true);
}

void Profiler::shutdown(VkDevice device) {
    if (query_pool_ != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device, query_pool_, nullptr);
        query_pool_ = VK_NULL_HANDLE;
        spdlog::info("Profiler query pool destroyed");
    }

    history_.clear();
    gpu_sections_.clear();
    prev_gpu_sections_.clear();
    cpu_section_starts_.clear();
}

// ---------------------------------------------------------------------------
// Frame begin / end
// ---------------------------------------------------------------------------

void Profiler::begin_frame() {
    current_frame_ = FrameProfile{};
    frame_start_ = Clock::now();

    // Swap GPU section lists: current becomes previous (for readback)
    prev_gpu_sections_ = std::move(gpu_sections_);
    gpu_sections_.clear();
    next_query_ = 0;
}

void Profiler::end_frame() {
    auto frame_end = Clock::now();
    auto elapsed = std::chrono::duration<float, std::milli>(frame_end - frame_start_);
    current_frame_.total_frame_ms = elapsed.count();

    if (current_frame_.total_frame_ms > 0.0f) {
        current_frame_.fps = 1000.0f / current_frame_.total_frame_ms;
    }

    // Sum CPU section times
    float cpu_total = 0.0f;
    for (const auto& entry : current_frame_.cpu_timings) {
        cpu_total += entry.duration_ms;
    }
    current_frame_.cpu_time_ms = cpu_total;

    // Sum GPU section times
    float gpu_total = 0.0f;
    for (const auto& entry : current_frame_.gpu_timings) {
        gpu_total += entry.duration_ms;
    }
    current_frame_.gpu_time_ms = gpu_total;

    // Add to history
    history_.push_back(current_frame_);
    if (history_.size() > max_history_) {
        history_.erase(history_.begin());
    }
}

// ---------------------------------------------------------------------------
// CPU timing
// ---------------------------------------------------------------------------

void Profiler::begin_cpu_section(const std::string& name) {
    cpu_section_starts_[name] = Clock::now();
}

void Profiler::end_cpu_section(const std::string& name) {
    auto it = cpu_section_starts_.find(name);
    if (it == cpu_section_starts_.end()) {
        spdlog::warn("Profiler: end_cpu_section('{}') without matching begin", name);
        return;
    }

    auto elapsed = std::chrono::duration<float, std::milli>(Clock::now() - it->second);

    FrameProfile::TimingEntry entry;
    entry.name = name;
    entry.duration_ms = elapsed.count();
    auto frame_elapsed = std::chrono::duration<float, std::milli>(it->second - frame_start_);
    entry.start_ms = frame_elapsed.count();

    current_frame_.cpu_timings.push_back(std::move(entry));
    cpu_section_starts_.erase(it);
}

// ---------------------------------------------------------------------------
// GPU timing
// ---------------------------------------------------------------------------

void Profiler::begin_gpu_section(VkCommandBuffer cmd, const std::string& name) {
    if (query_pool_ == VK_NULL_HANDLE) {
        return;
    }

    if (next_query_ + 1 >= max_timestamps_) {
        spdlog::warn("Profiler: out of timestamp queries (max={})", max_timestamps_);
        return;
    }

    uint32_t start_query = next_query_++;

    // Reset and write start timestamp
    vkCmdResetQueryPool(cmd, query_pool_, start_query, 1);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_, start_query);

    GpuSection section;
    section.name = name;
    section.start_query = start_query;
    section.end_query = UINT32_MAX; // will be filled in by end_gpu_section
    gpu_sections_.push_back(std::move(section));
}

void Profiler::end_gpu_section(VkCommandBuffer cmd, const std::string& name) {
    if (query_pool_ == VK_NULL_HANDLE) {
        return;
    }

    // Find matching section (search from back for nested sections)
    for (auto it = gpu_sections_.rbegin(); it != gpu_sections_.rend(); ++it) {
        if (it->name == name && it->end_query == UINT32_MAX) {
            uint32_t end_query = next_query_++;

            vkCmdResetQueryPool(cmd, query_pool_, end_query, 1);
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                query_pool_, end_query);

            it->end_query = end_query;
            return;
        }
    }

    spdlog::warn("Profiler: end_gpu_section('{}') without matching begin", name);
}

// ---------------------------------------------------------------------------
// Collect GPU timestamps from previous frame
// ---------------------------------------------------------------------------

void Profiler::collect_gpu_timestamps(VkDevice device) {
    if (query_pool_ == VK_NULL_HANDLE || prev_gpu_sections_.empty()) {
        return;
    }

    // Determine the range of queries to read back
    uint32_t max_query = 0;
    for (const auto& section : prev_gpu_sections_) {
        max_query = std::max(max_query, section.start_query);
        if (section.end_query != UINT32_MAX) {
            max_query = std::max(max_query, section.end_query);
        }
    }

    uint32_t query_count = max_query + 1;
    std::vector<uint64_t> timestamps(query_count, 0);

    VkResult vk_result = vkGetQueryPoolResults(
        device, query_pool_,
        0, query_count,
        query_count * sizeof(uint64_t),
        timestamps.data(),
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );

    if (vk_result != VK_SUCCESS && vk_result != VK_NOT_READY) {
        spdlog::warn("Profiler: vkGetQueryPoolResults returned {}", static_cast<int>(vk_result));
        return;
    }

    // Convert timestamps to timing entries
    for (const auto& section : prev_gpu_sections_) {
        if (section.end_query == UINT32_MAX) {
            continue; // Section was never closed
        }

        uint64_t start = timestamps[section.start_query];
        uint64_t end = timestamps[section.end_query];

        FrameProfile::TimingEntry entry;
        entry.name = section.name;
        // Convert ticks to milliseconds: ticks * period_ns / 1e6
        entry.duration_ms = static_cast<float>(end - start) * timestamp_period_ / 1e6f;
        entry.start_ms = static_cast<float>(start) * timestamp_period_ / 1e6f;

        current_frame_.gpu_timings.push_back(std::move(entry));
    }
}

// ---------------------------------------------------------------------------
// Get aggregate stats
// ---------------------------------------------------------------------------

ProfileStats Profiler::get_stats() const {
    return compute_profile_stats(history_);
}

} // namespace odyssey::debug
