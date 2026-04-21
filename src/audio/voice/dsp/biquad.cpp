//
// biquad.cpp — RBJ Audio EQ Cookbook implementations.
//
// All coefficient math is derived inline from the cookbook (hereafter "RBJ"):
// https://www.w3.org/TR/audio-eq-cookbook/. Every block cites the section
// number so a reviewer can check our algebra against the source.
//

#include "audio/voice/dsp/biquad.h"

#include <cmath>

namespace odyssey::audio::voice::dsp {

namespace {

// ---------------------------------------------------------------------------
// finite — reject NaN and +/-Inf. std::isfinite is the portable check; on
// MSVC this is <cmath> and compiles down to an fpclassify/ucomi sequence.
// ---------------------------------------------------------------------------
inline bool finite(float x) noexcept { return std::isfinite(x); }

} // namespace

// ---------------------------------------------------------------------------
// peaking_eq — RBJ §8.5.
//
// Intermediate variables (RBJ nomenclature, §2 "Variable reference"):
//   A      = 10^(gain_db / 40)               (amplitude ratio for peaking/shelf)
//   w0     = 2π · f0 / fs
//   alpha  = sin(w0) / (2Q)                  (bandwidth controlled by Q)
//   cos_w0 = cos(w0)
//
// Raw coefficients (RBJ §8.5 "peakingEQ"):
//   b0 =  1 + alpha · A
//   b1 = -2 · cos_w0
//   b2 =  1 - alpha · A
//   a0 =  1 + alpha / A
//   a1 = -2 · cos_w0
//   a2 =  1 - alpha / A
//
// We normalize by a0 so the stored difference equation has a0 == 1. This
// keeps the state update a single multiply per output sample. The A term
// is derived from the decibel definition:
//
//   gain_db = 20 · log10(linear_gain)
//   linear_gain = 10^(gain_db / 20)
//
// RBJ uses A^2 at the peak (the peaking_eq peak gain is A^2, not A), so
// the power-of-10 denominator is 40 instead of 20. Derivation: substituting
// w = w0 into H(z) and simplifying with alpha small gives |H(w0)| = A^2, so
// picking A = 10^(dB/40) hits the requested dB at peak. (Cross-checked
// against RBJ §8.5 footnote 2.)
// ---------------------------------------------------------------------------
Result<BiquadCoeffs, FilterError> peaking_eq(float fs, float f0, float Q, float gain_db) {
    if (!finite(fs) || fs <= 0.0f)      return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidSampleRate);
    if (!finite(f0) || f0 <= 0.0f)      return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidFrequency);
    if (f0 >= 0.5f * fs)                return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidFrequency);
    if (!finite(Q)  || Q  <= 0.0f)      return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidQ);
    if (!finite(gain_db))               return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidGain);

    const double PI = 3.14159265358979323846;
    const double A      = std::pow(10.0, static_cast<double>(gain_db) / 40.0);
    const double w0     = 2.0 * PI * static_cast<double>(f0) / static_cast<double>(fs);
    const double cos_w0 = std::cos(w0);
    const double sin_w0 = std::sin(w0);
    const double alpha  = sin_w0 / (2.0 * static_cast<double>(Q));

    // Raw cookbook coefficients.
    const double b0 =  1.0 + alpha * A;
    const double b1 = -2.0 * cos_w0;
    const double b2 =  1.0 - alpha * A;
    const double a0 =  1.0 + alpha / A;
    const double a1 = -2.0 * cos_w0;
    const double a2 =  1.0 - alpha / A;

    // Normalize by a0 so stored filter assumes a0 == 1.
    BiquadCoeffs c;
    c.b0 = static_cast<float>(b0 / a0);
    c.b1 = static_cast<float>(b1 / a0);
    c.b2 = static_cast<float>(b2 / a0);
    c.a1 = static_cast<float>(a1 / a0);
    c.a2 = static_cast<float>(a2 / a0);

    // Final sanity: if any coefficient is non-finite (can happen for extreme
    // f0 approaching Nyquist with tiny Q) we refuse rather than return trash.
    if (!finite(c.b0) || !finite(c.b1) || !finite(c.b2) ||
        !finite(c.a1) || !finite(c.a2)) {
        return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidFrequency);
    }
    return Result<BiquadCoeffs, FilterError>::ok(c);
}

// ---------------------------------------------------------------------------
// lowpass — RBJ §8.3 "LPF".
//
//   w0     = 2π · f0 / fs
//   alpha  = sin(w0) / (2Q)
//   cos_w0 = cos(w0)
//
//   b0 = (1 - cos_w0) / 2
//   b1 =  1 - cos_w0
//   b2 = (1 - cos_w0) / 2
//   a0 =  1 + alpha
//   a1 = -2 · cos_w0
//   a2 =  1 - alpha
//
// Derivation sketch: RBJ builds LPF from the analog prototype H_a(s) = 1/(s²+s/Q+1)
// via the bilinear transform s → (2/T)(1-z^-1)/(1+z^-1) with T = 1/fs.
// Prewarping the cutoff w0 into the z-domain yields the six coefficients
// above. See RBJ §3 "Bilinear transform" for the full derivation.
// ---------------------------------------------------------------------------
Result<BiquadCoeffs, FilterError> lowpass(float fs, float f0, float Q) {
    if (!finite(fs) || fs <= 0.0f) return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidSampleRate);
    if (!finite(f0) || f0 <= 0.0f) return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidFrequency);
    if (f0 >= 0.5f * fs)           return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidFrequency);
    if (!finite(Q)  || Q  <= 0.0f) return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidQ);

    const double PI = 3.14159265358979323846;
    const double w0     = 2.0 * PI * static_cast<double>(f0) / static_cast<double>(fs);
    const double cos_w0 = std::cos(w0);
    const double sin_w0 = std::sin(w0);
    const double alpha  = sin_w0 / (2.0 * static_cast<double>(Q));

    const double b0 = (1.0 - cos_w0) * 0.5;
    const double b1 =  1.0 - cos_w0;
    const double b2 = (1.0 - cos_w0) * 0.5;
    const double a0 =  1.0 + alpha;
    const double a1 = -2.0 * cos_w0;
    const double a2 =  1.0 - alpha;

    BiquadCoeffs c;
    c.b0 = static_cast<float>(b0 / a0);
    c.b1 = static_cast<float>(b1 / a0);
    c.b2 = static_cast<float>(b2 / a0);
    c.a1 = static_cast<float>(a1 / a0);
    c.a2 = static_cast<float>(a2 / a0);

    if (!finite(c.b0) || !finite(c.b1) || !finite(c.b2) ||
        !finite(c.a1) || !finite(c.a2)) {
        return Result<BiquadCoeffs, FilterError>::err(FilterError::InvalidFrequency);
    }
    return Result<BiquadCoeffs, FilterError>::ok(c);
}

// ---------------------------------------------------------------------------
// process_unchecked — transposed direct form II. Zölzer §2.4.3.
//
//   y    = b0 * x + z1
//   z1'  = b1 * x - a1 * y + z2
//   z2'  = b2 * x - a2 * y
//
// Order matters: y uses the OLD z1, then z1/z2 are updated using that y and
// the OLD z2. The commented swap order below matches the standard form.
// ---------------------------------------------------------------------------
float process_unchecked(const BiquadCoeffs& c, BiquadState& s, float x) noexcept {
    const float y = c.b0 * x + s.z1;
    const float new_z1 = c.b1 * x - c.a1 * y + s.z2;
    const float new_z2 = c.b2 * x - c.a2 * y;
    s.z1 = new_z1;
    s.z2 = new_z2;
    return y;
}

// ---------------------------------------------------------------------------
// process — safe variant: reject NaN/Inf input rather than silently
// propagating it through the state. A single NaN sample otherwise
// permanently poisons the filter memory.
// ---------------------------------------------------------------------------
Result<float, FilterError> process(const BiquadCoeffs& c, BiquadState& s, float x) {
    if (!finite(x)) return Result<float, FilterError>::err(FilterError::NanInput);
    return Result<float, FilterError>::ok(process_unchecked(c, s, x));
}

} // namespace odyssey::audio::voice::dsp
