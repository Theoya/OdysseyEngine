#pragma once
//
// voice_bus.h — the VoiceBus orchestrator.
//
// Owns the listener-centric view of the proximity-voice mix. Pure math lives
// in preview_for_listener() — it is the function council-ratified as the
// public API. The I/O side (WASAPI capture/playback, network relay) is wired
// through by Engine::tick and the ducker/envelope drives the MusicDirector
// sidechain EQ carve per Marty KB §11.
//
// Contract from docs/decisions/2026-04-20-proximity-voice-chat.md:
//   - preview_for_listener is pure: inputs fully describe the mix for the
//     target listener; output is the per-source mix parameters.
//   - DSP primitives (attenuation, pan, LPF, biquad, ducker) live in dsp/.
//   - WASAPI IO lives in io/.
//   - Codec + jitter buffer live under src/audio/voice/ but are per-speaker.
//
// This header intentionally does not expose Opus, WASAPI, or socket types.
//

#include "core/result.h"
#include "audio/voice/dsp/spatializer.h"
#include "audio/voice/dsp/listener.h"
#include "audio/voice/dsp/ducker.h"

#include <cstdint>
#include <string>
#include <vector>

namespace odyssey::audio::voice {

// A single active speaker as seen by one listener on one frame. Populated by
// the relay path + codec; preview_for_listener consumes it.
struct SpeakerSnapshot {
    uint32_t speaker_entity_id = 0;
    dsp::ListenerPose pose{};     // speaker pose (position + forward/right/up)
    float    voice_range_m = 25.0f; // per-entity stat, see schema
    float    occlusion_0_to_1 = 0.0f; // raycast-derived in Phase 2; 0 today
    bool     muted_by_listener = false; // per-listener mute list (UI-side)
};

// Per-speaker mixing parameters the audio thread applies.
struct MixedSource {
    uint32_t speaker_entity_id = 0;
    float    distance_m = 0.0f;
    float    attenuation_linear = 0.0f; // from distance_attenuation
    float    pan_L = 0.7071f;           // equal-power pan
    float    pan_R = 0.7071f;
    float    lpf_cutoff_hz = 20000.0f;  // distance + rear-hemisphere stacked
    bool     rear_hemisphere = false;
    bool     active = true;             // dropped if out of range / muted
};

// Final listener-frame mix description — everything the audio thread needs
// to actually mix this frame WITHOUT reaching back into this header again.
struct ListenerMix {
    std::vector<MixedSource>    sources;
    dsp::DuckerEnvelope         duck_envelope; // drives MusicDirector sidechain
    uint32_t                    speakers_in_range = 0;
    uint32_t                    speakers_dropped = 0; // out-of-range or muted
};

enum class VoiceBusError : uint32_t {
    InvalidListener = 1, // listener pose non-finite
    TooManySpeakers = 2, // more than kMaxSpeakers (budget protection)
    InvalidParams   = 3, // ducker / spatializer param failure
};

constexpr uint32_t kMaxSpeakers = 32; // council-ratified: voice mix must stay
                                       // under 0.5 ms for 32 concurrent sources
                                       // on RTX 3080 (architect addition).

// preview_for_listener — the PURE public API council-ratified as the single
// entry point for the listener-side mix computation.
//
// Inputs:
//   listener         — the listener's pose (usually the local player).
//   speakers         — the set of active speakers with their snapshots.
//   range            — distance attenuation parameters (d_min, d_max, taper).
//   ducker_state     — caller-owned running envelope state.
//   ducker_params    — tunable per-scene (Marty's tunable-sidechain condition).
//   dt_seconds       — frame delta for the ducker envelope.
//
// Output:
//   ListenerMix describing every active source's parameters + the duck env
//   the MusicDirector will consume this frame.
//
// Pure on (inputs, ducker_state_in) → (result, ducker_state_out). The only
// side effect is mutation of the caller-owned DuckerState, which matches the
// engine's "pure-with-caller-owned-state" pattern (same as BiquadState,
// VadState).
Result<ListenerMix, VoiceBusError>
preview_for_listener(const dsp::ListenerPose&                  listener,
                     const std::vector<SpeakerSnapshot>&       speakers,
                     const dsp::VoiceRangeParams&              range,
                     dsp::DuckerState&                         ducker_state,
                     const dsp::DuckerParams&                  ducker_params,
                     float                                     dt_seconds);

} // namespace odyssey::audio::voice
