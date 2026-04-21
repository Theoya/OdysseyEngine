#pragma once
//
// resample.h — minimal linear interpolation resampler, used *only* on the
// WASAPI path when the endpoint cannot deliver 48 kHz natively (very rare
// with AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM enabled). This is the sole DSP
// exception outside `dsp/` because it is topologically part of the I/O
// boundary — it adapts the device stream to the engine's canonical rate.
//
// Linear interpolation is audibly imperfect (~40 dB stopband). We accept
// that: the Opus encoder + human ears on a headset + VAD gate on noise all
// forgive the spectral artifacts introduced here. If we ever run into
// quality complaints (unlikely — WASAPI AUTOCONVERT almost always handles
// the conversion itself), swap this for a polyphase windowed-sinc.
//

#include "core/result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <variant>

namespace odyssey::audio::voice::io {

struct LinearResamplerState {
    double phase{0.0};          // fractional position in [0, 1) within current input span
    float  last_sample{0.0f};   // last sample of previous input buffer (for phase < previous-end)
    double step{1.0};           // input/output ratio (in_rate / out_rate)
};

enum class ResampleError : uint32_t {
    InvalidRate    = 1, // in_rate or out_rate <= 0
    EmptyInput     = 2,
};

// ---------------------------------------------------------------------------
// linear_resample_mono — resample `in` from `in_rate` Hz to `out_rate` Hz.
// Appends to `out` (does not clear it). Stateful through LinearResamplerState
// so chunked calls are continuous across buffer boundaries.
//
// Derivation.
//   Output sample at phase φ ∈ [0, 1) between input samples x[i] and x[i+1]:
//       y = (1 - φ) · x[i] + φ · x[i+1]
//   Advance φ by step = in_rate / out_rate per output sample; when φ ≥ 1,
//   bump i by ⌊φ⌋ and keep fractional part.
// ---------------------------------------------------------------------------
Result<std::monostate, ResampleError>
linear_resample_mono(std::span<const float> in,
                     uint32_t in_rate,
                     uint32_t out_rate,
                     LinearResamplerState& state,
                     std::vector<float>& out);

} // namespace odyssey::audio::voice::io
