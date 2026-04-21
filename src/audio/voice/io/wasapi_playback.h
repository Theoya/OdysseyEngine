#pragma once
//
// wasapi_playback.h — default-render-endpoint WASAPI playback for mixed
// proximity voice output. The mixer (owned by the architect in round 2)
// produces 48 kHz stereo float32 frames via the spatializer; this class
// pushes those frames to the OS audio endpoint.
//
// Threading: one dedicated render thread per WasapiPlayback instance. The
// thread waits on WASAPI's event, calls a caller-supplied FillCallback to
// obtain the next frame of stereo samples, and writes them to the audio
// client's render buffer.
//
// Lifecycle: start() → thread runs until stop() or destructor. Hotplug of
// the default render endpoint triggers a tear-down + re-initialize without
// tearing the thread down, same pattern as capture.
//

#include "core/result.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace odyssey::audio::voice::io {

enum class PlaybackError : uint32_t {
    CoInitFailed         = 1,
    EnumeratorCreateFail = 2,
    GetEndpointFail      = 3,
    ActivateClientFail   = 4,
    ClientInitializeFail = 5,
    GetServiceFail       = 6,
    SetEventHandleFail   = 7,
    StartStreamFail      = 8,
    FormatUnsupported    = 9,
    ThreadAlreadyRunning = 10,
    ThreadNotRunning     = 11,
    NullCallback         = 12,
};

struct PlaybackConfig {
    uint32_t sample_rate_hz = 48000;
    uint32_t channels       = 2;    // stereo output for the spatializer's mix
    uint32_t frame_samples  = 960;  // 20 ms at 48 kHz (per channel)
    uint32_t buffer_ms      = 60;   // 60 ms endpoint buffer — 3 frames of headroom
};

// ---------------------------------------------------------------------------
// FillCallback — invoked from the render thread to produce the next frame's
// worth of interleaved stereo float32 samples. The callback MUST write
// exactly `needed_samples * channels` floats into `out`; if the mixer has
// no voice to play this frame it should write zeros.
//
// Must not block on WASAPI or network.
// ---------------------------------------------------------------------------
using FillCallback = std::function<void(std::span<float> out_interleaved,
                                        uint32_t frames_needed,
                                        uint32_t channels)>;

struct PlaybackImpl;

class WasapiPlayback {
public:
    WasapiPlayback();
    ~WasapiPlayback();

    WasapiPlayback(const WasapiPlayback&) = delete;
    WasapiPlayback& operator=(const WasapiPlayback&) = delete;

    Result<std::monostate, PlaybackError> start(const PlaybackConfig& cfg, FillCallback cb);
    Result<std::monostate, PlaybackError> stop();

    bool        is_running() const noexcept;
    std::string current_device_name() const;
    bool        was_device_recently_switched() const noexcept;

private:
    std::unique_ptr<PlaybackImpl> impl_;
};

} // namespace odyssey::audio::voice::io
