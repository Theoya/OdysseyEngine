//
// spatializer.cpp — pure 3D voice spatialization math.
// Derivation comments in spatializer.h; here we keep the code tight.
//

#include "audio/voice/dsp/spatializer.h"

#include <algorithm>
#include <cmath>

namespace odyssey::audio::voice::dsp {

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double HALF_PI = 1.57079632679489661923;

inline bool finite(float x) noexcept { return std::isfinite(x); }

// Range-params validity — checked once per call; not hot-pathed.
bool range_ok(const VoiceRangeParams& r) noexcept {
    if (!finite(r.d_min) || !finite(r.d_max) || !finite(r.taper_width)) return false;
    if (r.d_min <= 0.0f) return false;
    if (r.d_max <= r.d_min) return false;
    if (r.taper_width < 0.0f) return false;
    if (r.taper_width > (r.d_max - r.d_min)) return false;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// distance_attenuation
// ---------------------------------------------------------------------------
Result<float, SpatializerError> distance_attenuation(float d, const VoiceRangeParams& r) {
    if (!finite(d) || d < 0.0f) return Result<float, SpatializerError>::err(SpatializerError::InvalidDistance);
    if (!range_ok(r))           return Result<float, SpatializerError>::err(SpatializerError::InvalidRange);

    // Past d_max → silence.
    if (d >= r.d_max) return Result<float, SpatializerError>::ok(0.0f);

    // Inside d_min bubble → unity.
    if (d <= r.d_min) return Result<float, SpatializerError>::ok(1.0f);

    // Inverse-amplitude core: a = d_min / d, monotone decreasing on (d_min, ∞).
    float a = r.d_min / d;

    // cos² taper over the last `taper_width` meters before d_max.
    // u = 0 at (d_max - taper_width), u = 1 at d_max.
    const float taper_start = r.d_max - r.taper_width;
    if (d > taper_start) {
        const float u = (d - taper_start) / r.taper_width;                 // ∈ (0, 1]
        const float c = static_cast<float>(std::cos(u * HALF_PI));         // cos(0) = 1 → cos(π/2) = 0
        a *= c * c;                                                         // cos²
    }

    // Numeric sanity: clamp to [0, 1] — the formula is already bounded but
    // floating point can drift by an ulp, and we don't want out-of-range
    // gains to multiply into the mixer.
    a = std::clamp(a, 0.0f, 1.0f);
    return Result<float, SpatializerError>::ok(a);
}

// ---------------------------------------------------------------------------
// equal_power_pan
// ---------------------------------------------------------------------------
Result<PanPair, SpatializerError> equal_power_pan(float theta_rad) {
    if (!finite(theta_rad)) return Result<PanPair, SpatializerError>::err(SpatializerError::InvalidAzimuth);

    // Rear fold: the pan law is a front-hemisphere law. For rear sources we
    // mirror θ across the ±π/2 boundary so the left/right sense is preserved
    // (a source at 3π/4 — right-behind — still pans right, not left).
    //   θ > +π/2  → θ' =  π - θ   (keeps sign of "right")
    //   θ < -π/2  → θ' = -π - θ   (keeps sign of "left")
    float theta = theta_rad;
    if (theta >  static_cast<float>(HALF_PI)) theta =  static_cast<float>(PI) - theta;
    if (theta < -static_cast<float>(HALF_PI)) theta = -static_cast<float>(PI) - theta;

    // φ = θ/2 + π/4, mapping θ ∈ [-π/2, +π/2] to φ ∈ [0, π/2].
    const double phi = 0.5 * static_cast<double>(theta) + 0.25 * PI;

    PanPair p;
    p.L = static_cast<float>(std::cos(phi));
    p.R = static_cast<float>(std::sin(phi));

    // Numerical hygiene: clamp to [0, 1]. cos/sin of a clean real φ can't
    // exceed 1 analytically but can overshoot by ulps.
    p.L = std::clamp(p.L, 0.0f, 1.0f);
    p.R = std::clamp(p.R, 0.0f, 1.0f);
    return Result<PanPair, SpatializerError>::ok(p);
}

// ---------------------------------------------------------------------------
// distance_lpf_cutoff
// ---------------------------------------------------------------------------
Result<float, SpatializerError>
distance_lpf_cutoff(float d, float occ, const VoiceRangeParams& r) {
    if (!finite(d)   || d < 0.0f)          return Result<float, SpatializerError>::err(SpatializerError::InvalidDistance);
    if (!finite(occ) || occ < 0.0f || occ > 1.0f) return Result<float, SpatializerError>::err(SpatializerError::InvalidOcclusion);
    if (!range_ok(r))                      return Result<float, SpatializerError>::err(SpatializerError::InvalidRange);

    // Distance ratio capped at 1 — past d_max the attenuation already
    // silenced the source, but a caller that calls us anyway should not
    // underflow the cutoff to negative numbers.
    const float d_ratio = std::min(d / r.d_max, 1.0f);

    float fc = 20000.0f - 12000.0f * d_ratio - 6000.0f * occ;
    fc = std::clamp(fc, 700.0f, 20000.0f);
    return Result<float, SpatializerError>::ok(fc);
}

} // namespace odyssey::audio::voice::dsp
