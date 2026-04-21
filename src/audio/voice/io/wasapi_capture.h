#pragma once
//
// wasapi_capture.h — Windows WASAPI (shared mode, communications role) mic
// capture for proximity voice chat. This is the I/O boundary: all filtering,
// VAD, encoding, and spatialization happen in pure DSP layers upstream of
// the callback invoked from here.
//
// Target format: 48 kHz, mono, float32, 20 ms frames (960 samples).
// If the default communications-role endpoint does not support float32
// directly we fall back to AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM +
// SRC_DEFAULT_QUALITY and request float32 anyway; WASAPI mixes formats for
// us. Very rare that the endpoint can't do float — but the resampler in
// resample.{h,cpp} is a belt-and-suspenders fallback if we ever disable
// AUTOCONVERT for exclusive-mode experiments.
//
// Threading model: one dedicated capture thread per WasapiCapture instance.
// The thread waits on WASAPI's event handle and calls a caller-supplied
// FrameCallback with full 960-sample frames re-chunked out of WASAPI's
// internal buffer (which may hand us 0-4 packets per wake).
//
// Lifetime: start() → thread runs until stop() is called or the object is
// destroyed. Device hotplug (OnDefaultDeviceChanged) is detected via a
// registered IMMNotificationClient and handled by tearing down and
// re-initializing the IAudioClient mid-run without tearing the thread down.
//

#include "core/result.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace odyssey::audio::voice::io {

// ---------------------------------------------------------------------------
// CaptureError — why start() failed (or the thread died).
// ---------------------------------------------------------------------------
enum class CaptureError : uint32_t {
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

struct CaptureConfig {
    uint32_t sample_rate_hz = 48000;
    uint32_t channels       = 1;
    uint32_t frame_samples  = 960;  // 20 ms at 48 kHz
    uint32_t buffer_ms      = 200;  // requested endpoint buffer
};

// ---------------------------------------------------------------------------
// FrameCallback — invoked from the capture thread with a mono float32 frame
// of exactly config.frame_samples samples. Callback must not block on
// WASAPI or network; push into a lock-free queue and return.
//
// capture_timestamp_qpc is QueryPerformanceCounter ticks at the frame's
// capture instant (used for jitter measurement / A-V sync).
// ---------------------------------------------------------------------------
using FrameCallback = std::function<void(std::span<const float> mono_frame_48k,
                                         uint64_t capture_timestamp_qpc)>;

// Forward decl — implementation lives in the .cpp file with COM types.
struct CaptureImpl;

class WasapiCapture {
public:
    WasapiCapture();
    ~WasapiCapture();

    WasapiCapture(const WasapiCapture&) = delete;
    WasapiCapture& operator=(const WasapiCapture&) = delete;

    // Start a capture thread. Returns err if WASAPI initialization fails or
    // a thread is already running.
    Result<std::monostate, CaptureError> start(const CaptureConfig& cfg, FrameCallback cb);

    // Stop the capture thread. Safe to call from any thread. Join is
    // implicit — this blocks until the worker is done.
    Result<std::monostate, CaptureError> stop();

    // True while the worker thread is running.
    bool is_running() const noexcept;

    // Human-readable endpoint name last acquired (for mixer-dump and the
    // voice-test-local CLI skill). Empty before start(), empty if hotplug
    // tore it down.
    std::string current_device_name() const;

    // True if the default-communications capture endpoint changed since
    // start. The capture thread responds by re-initializing mid-run; this
    // flag is read-only for external observers (HUD, overlay).
    bool was_device_recently_switched() const noexcept;

private:
    std::unique_ptr<CaptureImpl> impl_;
};

} // namespace odyssey::audio::voice::io
