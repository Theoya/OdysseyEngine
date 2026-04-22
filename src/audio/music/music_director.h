#pragma once
//
// music_director.h — MusicDirector subsystem public API.
//
// Contract (condition 10): This header exposes ONLY 4 ratified public APIs:
//   1. set_scene_theme(theme_id, transition_mode)
//   2. set_intensity_layers(intensity_values)
//   3. fire_stinger(stinger_id, intent, sync_mode)
//   4. tick(dt, world_state)
//
// Plus debug_snapshot() and current_mood_tag() for tooling.
//
// Internal modules (state_machine, bar_clock, leitmotif_table, sidechain_ducker)
// live under src/audio/music/detail/ and are internal headers only.
//
// Networking Deferred (condition 30):
//   The following events are candidates for future replication when a
//   NetMusicDirector wrapper is added:
//     - set_scene_theme(theme_id, ...) — broadcast to all clients
//     - fire_stinger(stinger_id, ...) — broadcast to all clients
//   Local-only (client-side, no replication):
//     - set_intensity_layers(...) — local ambient crossfade, not networked
//     - Ducker state and sidechain smoothing
//
// Nadir sound_request bit encoding (condition 13):
//   Nadir behaviors emit sound_request as u16 bitfield in output SSBO.
//   Bit 15 (0x8000): music-routing flag (0 = no request, 1 = has request)
//   Bits 14-12: category (0=stinger, 1=leitmotif, 2=intensity_hint, 3=theme, 4-7=reserved)
//   Bits 11-0: ID within category (0..4095)
//
// Stem Residency (condition 19):
//   Per-scene-theme stems fully resident (no streaming). Budget target <32 MB
//   decoded PCM per active theme. Use 48kHz 16-bit PCM WAV format only.
//

#include "core/result.h"
#include <cstdint>
#include <string>
#include <vector>

namespace odyssey::audio::music {

enum class TransitionMode : uint8_t {
    Immediate = 0,  // Fire now (CLI/testing)
    NextBeat = 1,   // Snap to next beat (combat urgency)
    NextBar = 2,    // Snap to next bar (default)
    NextPhrase = 3, // Snap to next 4-bar phrase
};

enum class StingerIntent : uint8_t {
    Reveal = 0,
    Punctuate = 1,
    Transition = 2,
    Sting = 3,
    Release = 4,
};

enum class MusicError : uint32_t {
    InvalidThemeID = 1,
    InvalidStingerID = 2,
    InvalidIntensityRange = 3,
    KeyMismatch = 4,
    UnresolvedLeitmotifRef = 5,
    StemFormatError = 6,
};

// Snapshot for debug overlay and queries.
struct MusicDirectorSnapshot {
    uint32_t current_bar = 0;
    uint32_t current_beat = 0;
    float bar_phase = 0.0f;  // [0, 1)
    uint32_t active_theme_id = 0;
    uint32_t active_stinger_id = 0;
    std::vector<float> intensity_layers;  // Per-stem current intensities
    std::string current_mood_tag;
};

//
// Public API: Four core functions.
//

// Unit struct for successful Result<Unit, E> returns (void-like).
struct Unit {};

// Set the active scene theme. Transitions snap to a quantization boundary.
Result<Unit, MusicError>
set_scene_theme(uint32_t theme_id, TransitionMode mode);

// Set intensity envelope targets for all layers. Layers crossfade smoothly.
Result<Unit, MusicError>
set_intensity_layers(const std::vector<float>& intensity_targets);

// Fire a stinger (one-shot accent). Snaps to bar boundary, respects min-silence.
Result<Unit, MusicError>
fire_stinger(uint32_t stinger_id, StingerIntent intent, TransitionMode sync);

// Advance the clock and publish MixPlan snapshot. Called once per frame by Engine::tick.
void tick(float dt_seconds);

//
// Debug and query APIs.
//

// Snapshot for debug overlay (current bar, beat, mood, active stinger).
MusicDirectorSnapshot debug_snapshot();

// Current mood_tag of active state. Used by post-FX / camera subscribers.
std::string current_mood_tag();

//
// Utility: Nadir sound_request decoder (condition 13).
//

inline bool has_music_request(uint16_t sound_request) noexcept {
    return (sound_request & 0x8000) != 0;
}

inline uint8_t decode_sound_request_category(uint16_t sound_request) noexcept {
    return (sound_request >> 12) & 0x7;
}

inline uint16_t decode_sound_request_id(uint16_t sound_request) noexcept {
    return sound_request & 0xFFF;
}

} // namespace odyssey::audio::music
