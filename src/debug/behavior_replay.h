#pragma once
#include "core/types.h"
#include "core/result.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace odyssey::debug {

// One frame of behavior data for one entity
struct BehaviorFrame {
    uint32_t frame_number;
    EntityID entity_id;
    BehaviorOutput output;
    AgentPersistState state;
};

// Complete replay recording
struct BehaviorRecording {
    std::string archetype_name;
    uint32_t entity_count;
    uint32_t frame_count;
    std::vector<BehaviorFrame> frames;
};

class BehaviorReplay {
public:
    // Start recording behavior outputs
    void start_recording(const std::string& archetype, uint32_t entity_count);

    // Record one frame of outputs (call each frame)
    void record_frame(uint32_t frame_number,
                      const std::vector<BehaviorOutput>& outputs,
                      const std::vector<AgentPersistState>& states);

    // Stop recording
    BehaviorRecording stop_recording();

    // Save recording to file (binary format)
    Result<bool> save_recording(const BehaviorRecording& recording,
                                const std::filesystem::path& path);

    // Load recording from file
    Result<BehaviorRecording> load_recording(const std::filesystem::path& path);

    // Playback: get frame data at a specific frame
    // Pure: extract data for a specific entity at a frame
    static const BehaviorFrame* get_frame(const BehaviorRecording& recording,
                                           uint32_t frame_number,
                                           EntityID entity_id);

    // Pure: compute statistics from recording
    struct RecordingStats {
        float avg_move_weight;
        float avg_attack_weight;
        float max_move_weight;
        uint32_t state_change_count;
        std::unordered_map<uint32_t, uint32_t> state_distribution; // state -> frame count
    };

    static RecordingStats compute_stats(const BehaviorRecording& recording, EntityID entity_id);

    bool is_recording() const { return recording_; }

private:
    bool recording_ = false;
    BehaviorRecording current_;
};

} // namespace odyssey::debug
