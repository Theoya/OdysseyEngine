#include "debug/behavior_replay.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace odyssey::debug {

// ---------------------------------------------------------------------------
// Binary file format magic and version
// ---------------------------------------------------------------------------

static constexpr uint32_t REPLAY_MAGIC = 0x4F445252; // "ODRR" - Odyssey Debug Replay Recording
static constexpr uint32_t REPLAY_VERSION = 1;

// File header structure (written at the start of the file)
struct ReplayFileHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t entity_count;
    uint32_t frame_count;
    uint32_t total_frames; // number of BehaviorFrame entries
    uint32_t archetype_name_length;
    // Followed by: archetype_name (archetype_name_length bytes, no null terminator)
    // Followed by: total_frames * BehaviorFrame
};

// ---------------------------------------------------------------------------
// Recording control
// ---------------------------------------------------------------------------

void BehaviorReplay::start_recording(const std::string& archetype, uint32_t entity_count) {
    if (recording_) {
        spdlog::warn("BehaviorReplay: already recording, stopping previous recording");
        stop_recording();
    }

    current_ = BehaviorRecording{};
    current_.archetype_name = archetype;
    current_.entity_count = entity_count;
    current_.frame_count = 0;
    recording_ = true;

    spdlog::info("BehaviorReplay: started recording archetype '{}' ({} entities)",
                 archetype, entity_count);
}

void BehaviorReplay::record_frame(uint32_t frame_number,
                                   const std::vector<BehaviorOutput>& outputs,
                                   const std::vector<AgentPersistState>& states) {
    if (!recording_) {
        return;
    }

    // Record one BehaviorFrame per entity
    uint32_t count = std::min(
        static_cast<uint32_t>(outputs.size()),
        static_cast<uint32_t>(states.size())
    );
    count = std::min(count, current_.entity_count);

    for (uint32_t i = 0; i < count; ++i) {
        BehaviorFrame frame;
        frame.frame_number = frame_number;
        frame.entity_id = i;
        frame.output = outputs[i];
        frame.state = states[i];
        current_.frames.push_back(std::move(frame));
    }

    current_.frame_count++;
}

BehaviorRecording BehaviorReplay::stop_recording() {
    recording_ = false;

    spdlog::info("BehaviorReplay: stopped recording '{}' ({} frames, {} entries)",
                 current_.archetype_name, current_.frame_count, current_.frames.size());

    return std::move(current_);
}

// ---------------------------------------------------------------------------
// Save / Load (binary format)
// ---------------------------------------------------------------------------

Result<bool> BehaviorReplay::save_recording(const BehaviorRecording& recording,
                                             const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Result<bool>::err("Failed to open file for writing: " + path.string());
    }

    // Write header
    ReplayFileHeader header{};
    header.magic = REPLAY_MAGIC;
    header.version = REPLAY_VERSION;
    header.entity_count = recording.entity_count;
    header.frame_count = recording.frame_count;
    header.total_frames = static_cast<uint32_t>(recording.frames.size());
    header.archetype_name_length = static_cast<uint32_t>(recording.archetype_name.size());

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!file.good()) {
        return Result<bool>::err("Failed to write header to: " + path.string());
    }

    // Write archetype name
    file.write(recording.archetype_name.data(),
               static_cast<std::streamsize>(recording.archetype_name.size()));

    // Write all frames
    for (const auto& frame : recording.frames) {
        file.write(reinterpret_cast<const char*>(&frame.frame_number), sizeof(frame.frame_number));
        file.write(reinterpret_cast<const char*>(&frame.entity_id), sizeof(frame.entity_id));
        file.write(reinterpret_cast<const char*>(&frame.output), sizeof(frame.output));
        file.write(reinterpret_cast<const char*>(&frame.state), sizeof(frame.state));
    }

    if (!file.good()) {
        return Result<bool>::err("Failed to write frame data to: " + path.string());
    }

    spdlog::info("BehaviorReplay: saved recording to {} ({} bytes)",
                 path.string(), static_cast<int64_t>(file.tellp()));
    return Result<bool>::ok(true);
}

Result<BehaviorRecording> BehaviorReplay::load_recording(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Result<BehaviorRecording>::err(
            "Failed to open file for reading: " + path.string());
    }

    // Read header
    ReplayFileHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good()) {
        return Result<BehaviorRecording>::err(
            "Failed to read header from: " + path.string());
    }

    // Validate magic
    if (header.magic != REPLAY_MAGIC) {
        return Result<BehaviorRecording>::err(
            "Invalid replay file magic in: " + path.string());
    }

    // Validate version
    if (header.version != REPLAY_VERSION) {
        return Result<BehaviorRecording>::err(
            "Unsupported replay file version " + std::to_string(header.version) +
            " in: " + path.string());
    }

    BehaviorRecording recording;
    recording.entity_count = header.entity_count;
    recording.frame_count = header.frame_count;

    // Read archetype name
    recording.archetype_name.resize(header.archetype_name_length);
    file.read(recording.archetype_name.data(),
              static_cast<std::streamsize>(header.archetype_name_length));

    // Read all frames
    recording.frames.resize(header.total_frames);
    for (uint32_t i = 0; i < header.total_frames; ++i) {
        auto& frame = recording.frames[i];
        file.read(reinterpret_cast<char*>(&frame.frame_number), sizeof(frame.frame_number));
        file.read(reinterpret_cast<char*>(&frame.entity_id), sizeof(frame.entity_id));
        file.read(reinterpret_cast<char*>(&frame.output), sizeof(frame.output));
        file.read(reinterpret_cast<char*>(&frame.state), sizeof(frame.state));
    }

    if (!file.good()) {
        return Result<BehaviorRecording>::err(
            "Failed to read frame data from: " + path.string());
    }

    spdlog::info("BehaviorReplay: loaded recording '{}' ({} frames, {} entries)",
                 recording.archetype_name, recording.frame_count,
                 recording.frames.size());

    return Result<BehaviorRecording>::ok(std::move(recording));
}

// ---------------------------------------------------------------------------
// Pure: get frame data at a specific frame for a specific entity
// ---------------------------------------------------------------------------

const BehaviorFrame* BehaviorReplay::get_frame(const BehaviorRecording& recording,
                                                uint32_t frame_number,
                                                EntityID entity_id) {
    for (const auto& frame : recording.frames) {
        if (frame.frame_number == frame_number && frame.entity_id == entity_id) {
            return &frame;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Pure: compute statistics from recording for a specific entity
// ---------------------------------------------------------------------------

BehaviorReplay::RecordingStats BehaviorReplay::compute_stats(
    const BehaviorRecording& recording, EntityID entity_id) {

    RecordingStats stats{};
    stats.avg_move_weight = 0.0f;
    stats.avg_attack_weight = 0.0f;
    stats.max_move_weight = 0.0f;
    stats.state_change_count = 0;

    // Collect all frames for this entity
    std::vector<const BehaviorFrame*> entity_frames;
    for (const auto& frame : recording.frames) {
        if (frame.entity_id == entity_id) {
            entity_frames.push_back(&frame);
        }
    }

    if (entity_frames.empty()) {
        return stats;
    }

    float total_move_weight = 0.0f;
    float total_attack_weight = 0.0f;
    float max_move = 0.0f;
    uint32_t prev_state = entity_frames[0]->state.current_state;

    for (const auto* frame : entity_frames) {
        float move_w = frame->output.move_vector.w;
        float attack_w = frame->output.attack_target.w;

        total_move_weight += move_w;
        total_attack_weight += attack_w;
        max_move = std::max(max_move, move_w);

        // Track state changes
        uint32_t current_state = frame->state.current_state;
        if (current_state != prev_state) {
            stats.state_change_count++;
            prev_state = current_state;
        }

        // Track state distribution
        stats.state_distribution[current_state]++;
    }

    auto n = static_cast<float>(entity_frames.size());
    stats.avg_move_weight = total_move_weight / n;
    stats.avg_attack_weight = total_attack_weight / n;
    stats.max_move_weight = max_move;

    return stats;
}

} // namespace odyssey::debug
