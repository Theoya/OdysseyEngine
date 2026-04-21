//
// vad.cpp — RMS + ZCR gate with hang-time hysteresis.
//

#include "audio/voice/dsp/vad.h"

#include <cmath>
#include <algorithm>

namespace odyssey::audio::voice::dsp {

namespace {

inline bool finite(float x) noexcept { return std::isfinite(x); }

inline int8_t sign_of(float x) noexcept {
    if (x > 0.0f) return +1;
    if (x < 0.0f) return -1;
    return 0;
}

bool params_ok(const VadParams& p) noexcept {
    if (!finite(p.enter_dbfs) || !finite(p.exit_dbfs)) return false;
    if (p.enter_dbfs <= p.exit_dbfs) return false;       // enter > exit (dBFS is negative-ish)
    if (!finite(p.min_zcr) || p.min_zcr < 0.0f) return false;
    if (!finite(p.hang_ms) || p.hang_ms < 0.0f) return false;
    if (!finite(p.frame_ms) || p.frame_ms <= 0.0f) return false;
    return true;
}

} // namespace

Result<VadDecision, VadError>
analyze(std::span<const float> frame,
        const VadParams& params,
        VadState& state) {
    if (frame.empty())                return Result<VadDecision, VadError>::err(VadError::EmptyFrame);
    if (frame.size() > 1024)          return Result<VadDecision, VadError>::err(VadError::FrameTooLong);
    if (!params_ok(params))           return Result<VadDecision, VadError>::err(VadError::InvalidParams);

    // ------------------------------------------------------------------
    // RMS accumulate. Sum of squares in double to avoid losing precision on
    // a 960-sample integration with float32 input in [-1, +1].
    // ------------------------------------------------------------------
    double sum_sq = 0.0;
    size_t zc = 0;
    int8_t prev_sign = state.last_sample_sign;

    for (float x : frame) {
        if (!finite(x)) return Result<VadDecision, VadError>::err(VadError::NonFiniteSample);
        sum_sq += static_cast<double>(x) * static_cast<double>(x);
        const int8_t s = sign_of(x);
        // Count a crossing whenever the sign *changes*; zero-to-positive or
        // zero-to-negative counts, but zero samples in the middle of a run
        // of identical signs do not spuriously add crossings.
        if (s != 0 && prev_sign != 0 && s != prev_sign) ++zc;
        if (s != 0) prev_sign = s;
    }

    const size_t N = frame.size();
    const double mean_sq = sum_sq / static_cast<double>(N);
    const double rms = std::sqrt(mean_sq);
    const double rms_floor = 1e-12; // -240 dBFS floor so log10 stays finite
    const double rms_dbfs = 20.0 * std::log10(std::max(rms, rms_floor));
    const double zcr = static_cast<double>(zc) / static_cast<double>(N);

    VadDecision dec;
    dec.rms_dbfs = static_cast<float>(rms_dbfs);
    dec.zcr      = static_cast<float>(zcr);

    // ------------------------------------------------------------------
    // Hysteresis state machine.
    //   hang_frames = ceil(hang_ms / frame_ms). Integer frames of carry.
    // ------------------------------------------------------------------
    const int hang_frames =
        static_cast<int>(std::ceil(static_cast<double>(params.hang_ms) /
                                   static_cast<double>(params.frame_ms)));

    const bool above_enter = (rms_dbfs > params.enter_dbfs) && (zcr > params.min_zcr);
    const bool above_exit  = (rms_dbfs > params.exit_dbfs);

    if (!state.active) {
        if (above_enter) {
            state.active = true;
            state.hang_frames_left = hang_frames;
        }
    } else {
        if (above_exit) {
            state.hang_frames_left = hang_frames; // refresh hang while signal lives
        } else {
            state.hang_frames_left -= 1;
            if (state.hang_frames_left <= 0) {
                state.active = false;
                state.hang_frames_left = 0;
            }
        }
    }

    // Persist the last nonzero sign for the next frame's boundary check.
    state.last_sample_sign = prev_sign;

    dec.active = state.active;
    return Result<VadDecision, VadError>::ok(dec);
}

} // namespace odyssey::audio::voice::dsp
