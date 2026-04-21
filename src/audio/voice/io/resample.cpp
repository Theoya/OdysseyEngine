//
// resample.cpp — linear interpolation mono resampler for the WASAPI path.
//

#include "audio/voice/io/resample.h"

namespace odyssey::audio::voice::io {

Result<std::monostate, ResampleError>
linear_resample_mono(std::span<const float> in,
                     uint32_t in_rate,
                     uint32_t out_rate,
                     LinearResamplerState& state,
                     std::vector<float>& out) {
    if (in_rate == 0 || out_rate == 0) return Result<std::monostate, ResampleError>::err(ResampleError::InvalidRate);
    if (in.empty())                    return Result<std::monostate, ResampleError>::err(ResampleError::EmptyInput);

    // Keep step fresh in case the caller swapped rates mid-stream.
    state.step = static_cast<double>(in_rate) / static_cast<double>(out_rate);

    // Position carried across frames: phase ∈ [0, in.size()). Integer part
    // indexes into `in`, fractional part is the interpolation alpha.
    // `phase` is relative to the *start of this buffer*; after finishing we
    // subtract in.size() to carry the remainder forward — the "negative
    // phase" represents how far into the boundary last_sample still matters.
    double phase = state.phase;
    const double step = state.step;

    const size_t N = in.size();
    // Output until `phase` falls past the end of this input span.
    while (phase < static_cast<double>(N)) {
        if (phase < 0.0) {
            // Interpolate across the previous buffer's boundary: last_sample
            // is effectively at index -1; in[0] is at index 0. alpha = phase+1.
            const double alpha = phase + 1.0;
            const float  y     = static_cast<float>((1.0 - alpha) * static_cast<double>(state.last_sample)
                                                   + alpha * static_cast<double>(in[0]));
            out.push_back(y);
        } else {
            const size_t i = static_cast<size_t>(phase);
            const double alpha = phase - static_cast<double>(i);
            const float x0 = in[i];
            const float x1 = (i + 1 < N) ? in[i + 1] : in[N - 1]; // hold last sample at buffer end
            const float y  = static_cast<float>((1.0 - alpha) * static_cast<double>(x0)
                                               + alpha * static_cast<double>(x1));
            out.push_back(y);
        }
        phase += step;
    }

    // Carry remaining phase forward, referenced to the *next* buffer: subtract
    // N so phase becomes negative (within (-1, 0]) if we straddled the boundary.
    state.phase       = phase - static_cast<double>(N);
    state.last_sample = in[N - 1];
    return Result<std::monostate, ResampleError>::ok(std::monostate{});
}

} // namespace odyssey::audio::voice::io
