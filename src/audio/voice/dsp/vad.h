#pragma once
//
// vad.h — pure voice activity detection (dual-threshold RMS + ZCR gate with
// hang-time hysteresis). Called once per captured 20 ms frame to decide
// whether the encoder should run and a voice packet should be transmitted.
//
// Design rationale: see docs/design/proximity_chat_audio.md §3. We use our
// own VAD rather than Opus internal VAD/DTX because we want deterministic
// gating decisions we can visualize (mic icon, mixer-dump), and because our
// threshold + hang behavior is tuned for game-social speech, not telephony.
//
// Pure: the analysis function is stateless; the hysteresis is captured in a
// caller-owned VadState. This is the "VadState mutates across calls but the
// function itself is pure on (frame, state_in)" pattern we use throughout
// the engine (Result<T,E>-returning, no globals, no I/O).
//

#include "core/result.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace odyssey::audio::voice::dsp {

// ---------------------------------------------------------------------------
// VadParams — tuned defaults documented in design doc §3.2.
// ---------------------------------------------------------------------------
struct VadParams {
    float enter_dbfs = -38.0f; // RMS above this opens the gate
    float exit_dbfs  = -45.0f; // RMS below this closes the gate (after hang)
    float min_zcr    = 0.02f;  // reject DC hum even if RMS passes
    float hang_ms    = 200.0f; // keep gate open N ms after signal drops
    float frame_ms   = 20.0f;  // frame duration (matches Opus 20 ms)
};

// ---------------------------------------------------------------------------
// VadState — cross-frame memory. One instance per capture stream.
//   last_sample_sign: needed so ZCR counts crossings at the frame boundary,
//                     otherwise every frame silently drops one true crossing.
//   hang_frames_left: decrements per frame while gate is held open past the
//                     exit threshold. 0 = closed, >0 = held open.
// ---------------------------------------------------------------------------
struct VadState {
    int8_t last_sample_sign = 0;  // -1, 0, +1 — previous frame's last sample
    int    hang_frames_left = 0;  // frames remaining in hang window
    bool   active           = false;
};

// ---------------------------------------------------------------------------
// VadError — rejection modes for analyze().
// ---------------------------------------------------------------------------
enum class VadError : uint32_t {
    EmptyFrame       = 1, // span is empty
    FrameTooLong     = 2, // frame exceeds 1024 samples (sanity — 20 ms @ 48k = 960)
    NonFiniteSample  = 3, // a sample is NaN/Inf
    InvalidParams    = 4, // enter <= exit, hang < 0, frame_ms <= 0, etc.
};

// ---------------------------------------------------------------------------
// VadDecision — richer than a bool; the ducker and the debug overlay want
// the RMS envelope so they can draw a VU-ish indicator, and the encoder
// handoff uses rms_dbfs in its telemetry.
// ---------------------------------------------------------------------------
struct VadDecision {
    bool  active{false};
    float rms_dbfs{-120.0f}; // current frame RMS in dBFS (-120 = effectively silent)
    float zcr{0.0f};         // zero crossing rate in [0, 1]
};

// ---------------------------------------------------------------------------
// analyze — pure function (on the state_in, state_out split): compute RMS
// and ZCR, apply the dual-threshold hysteresis with hang-time, update state.
//
// Derivation.
//   RMS for frame x[0..N-1]:
//       rms = sqrt( (1/N) Σ x[n]² )
//   dBFS: dBFS(rms) = 20 · log10(max(rms, 1e-12))
//       floor at 1e-12 so silent frames map to -240 dB rather than -∞.
//
//   ZCR:
//       zcr = (1 / N) · #{ n ∈ [1, N-1] : sign(x[n]) ≠ sign(x[n-1]) }
//       plus one boundary check against the previous frame's last sample.
//       sign(0) is treated as 0 (no crossing contribution).
//
//   Hysteresis:
//     if !active and rms > enter and zcr > min_zcr:
//         active = true, hang_frames_left = hang_frames
//     elif active:
//         if rms > exit:
//             hang_frames_left = hang_frames      (refresh hang)
//         else:
//             hang_frames_left -= 1
//             if hang_frames_left <= 0:
//                 active = false
//
//   hang_frames = ceil(hang_ms / frame_ms); at the defaults (200/20) = 10 frames.
//
// Returns:
//   Ok(VadDecision) on success.
//   Err(...)        for empty frame / NaN sample / invalid params.
// ---------------------------------------------------------------------------
Result<VadDecision, VadError>
analyze(std::span<const float> frame_48k_mono,
        const VadParams& params,
        VadState& state);

} // namespace odyssey::audio::voice::dsp
