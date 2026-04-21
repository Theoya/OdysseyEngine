#pragma once
//
// spatializer.h — pure 3D spatialization for proximity voice chat.
//
// Three components, each a pure function:
//   1. Distance attenuation (inverse-amplitude with near + far clamps + cos² taper)
//   2. Equal-power stereo pan from azimuth
//   3. Distance-driven LPF cutoff (air absorption + occlusion + rear-cue)
//
// The functions here do NOT apply any filter state — they return *parameters*
// (gains, cutoffs) that a caller (VoiceBus / per-source mixer) feeds into
// stateful objects (BiquadState, gain envelopes). This keeps the DSP math
// pure and trivially testable.
//
// Conventions:
//   - Azimuth θ in radians, signed. θ=0 is dead-ahead, θ=+π/2 is hard right.
//     (matches listener::source_azimuth).
//   - Distances in meters.
//   - Gains are linear, not dB, except where noted.
//

#include "core/result.h"

#include <cstdint>

namespace odyssey::audio::voice::dsp {

// ---------------------------------------------------------------------------
// Default voice-range parameters. These are tighter than the general SFX
// range because conversational voice must feel socially proximate; broadcast
// at 100 m destroys the "social distance cue" that proximity chat is for.
// Any per-entity override (scene-authored voice_range stat) substitutes for
// d_max only; d_min and the 5 m taper are fixed per design.
// ---------------------------------------------------------------------------
struct VoiceRangeParams {
    float d_min       = 1.0f;  // no gain above 1.0; inside this bubble, full volume
    float d_max       = 25.0f; // past this, silent
    float taper_width = 5.0f;  // cos² taper applied in [d_max - taper_width, d_max]
};

// ---------------------------------------------------------------------------
// SpatializerError — why a parameter query was rejected.
// ---------------------------------------------------------------------------
enum class SpatializerError : uint32_t {
    InvalidDistance = 1, // d < 0 or non-finite
    InvalidAzimuth  = 2, // θ non-finite
    InvalidRange    = 3, // d_min <= 0, d_max <= d_min, taper > (d_max - d_min), etc.
    InvalidOcclusion = 4, // occ outside [0, 1] or non-finite
};

// ---------------------------------------------------------------------------
// PanPair — left/right linear gains. Equal-power guarantees L² + R² == 1.
// ---------------------------------------------------------------------------
struct PanPair {
    float L{0.7071067811865476f};
    float R{0.7071067811865476f};
};

// ---------------------------------------------------------------------------
// distance_attenuation — inverse-amplitude attenuation with near + far
// clamps and a 5 m cos² taper ending at d_max.
//
// Derivation.
//   Sound intensity from a point source falls as 1/d² by the inverse-square
//   law (energy spreads over a sphere of area 4πd²).
//   Amplitude a = √intensity, so a ∝ 1/d.
//   1/d diverges as d → 0, so we clamp at d_min: for d ≤ d_min the source
//   is treated as co-located and gain = 1.0.
//   Beyond d_max the source is inaudible (gain = 0).
//
// The -3 dB factor-of-two note the design calls out: if we had used an
// inverse-power law (a ∝ 1/d²), doubling distance would attenuate by 12 dB.
// With inverse-amplitude (a ∝ 1/d), doubling distance attenuates by exactly
// 6 dB (20 · log10(1/2) = -6.02 dB). That 6-vs-12 dB per doubling is the
// factor of 2 in dB between the two conventions. Voice chat feels more
// natural on inverse-amplitude — far-away speakers aren't unnaturally
// whispered. (See: Zwicker & Fastl, "Psychoacoustics", ch. 3 on distance
// perception cues — monaural intensity is only one of several cues, so an
// overly aggressive falloff becomes uncanny.)
//
// Taper. Abruptly clamping to zero at d_max creates a jarring click as a
// source crosses the boundary. We multiply the last `taper_width` meters by
// a cos² window:
//
//   let u = (d - (d_max - taper_width)) / taper_width,   u ∈ [0, 1]
//   taper(d) = cos²(u · π/2)
//
// cos²(0) = 1, cos²(π/2) = 0 — smooth shoulder to silence.
//
// Returns:
//   Ok(a) where a ∈ [0, 1].
//   Err(InvalidDistance) if d is negative or non-finite.
//   Err(InvalidRange)    if VoiceRangeParams are inconsistent.
// ---------------------------------------------------------------------------
Result<float, SpatializerError> distance_attenuation(float d, const VoiceRangeParams& r);

// ---------------------------------------------------------------------------
// equal_power_pan — map signed azimuth θ (radians) to (L, R) gains.
//
// Derivation.
//   We want a pan law where L² + R² is constant across the pan arc, because
//   uncorrelated-ish sources summed by a listener's ears add in *power* (to
//   a first approximation on voice signals, Blauert "Spatial Hearing" §2.4).
//   A purely linear pan (L = 1-p, R = p) has L² + R² dip at center to 0.5;
//   signal dips 3 dB center-panned. Bad.
//
//   Fix: pick a "pan angle" φ ∈ [0, π/2] and set L = cos(φ), R = sin(φ).
//   By the Pythagorean identity sin²(φ) + cos²(φ) = 1, so L² + R² = 1 ∀ φ.
//
//   Map θ ∈ [-π/2, +π/2] to φ ∈ [0, π/2]:
//     φ = θ/2 + π/4
//   Check:
//     θ = -π/2 → φ = 0       → L = 1, R = 0          (hard left)
//     θ =  0   → φ = π/4     → L = R = √2/2 ≈ 0.707  (center)
//     θ = +π/2 → φ = π/2     → L = 0, R = 1          (hard right)
//
// Rear hemisphere (|θ| > π/2). Stereo pan alone cannot disambiguate
// front-vs-back. We fold θ into the front hemisphere for the pan law so the
// L/R split is monotone left-right regardless of in-front or behind, and
// the caller applies the rear attenuation + 1 kHz LPF cue separately (see
// rear_cue_gain + rear_lpf_cutoff_hz below).
//
// Returns:
//   Ok(PanPair)
//   Err(InvalidAzimuth) for non-finite θ.
// ---------------------------------------------------------------------------
Result<PanPair, SpatializerError> equal_power_pan(float theta_rad);

// ---------------------------------------------------------------------------
// distance_lpf_cutoff — map distance + occlusion to a low-pass cutoff
// frequency in Hz, for the per-source distance LPF.
//
// Formula (from design doc §6.3):
//   fc(d, occ) = clamp(
//       20000 - 12000 · (d / d_max) - 6000 · occ,
//       700, 20000)
//
// Derivation.
//   Air absorbs high frequencies more than low frequencies — absorption
//   coefficient scales ~f² for humid air at typical temperatures (ISO 9613-1).
//   A linear-in-distance cutoff walk is a first-order approximation of that
//   (the quadratic-in-frequency absorption → linear-in-distance cutoff drop
//   when you solve for the -3 dB point of a one-pole LPF). 12 kHz of drop
//   over 25 m is aggressive but matches Halo-era voice treatment: at d_max
//   voice sits in a muffled ~8 kHz band that still intelligibly conveys
//   phonemes but strips presence.
//
//   Occlusion coefficient occ ∈ [0,1] from raycast (phase 2); we add another
//   6 kHz of drop per unit occlusion. An occluded distant speaker lands
//   around 2 kHz — muffled but still a telephone-grade voice.
//
//   Floor at 700 Hz: fundamental vowel energy sits above 500 Hz; dropping
//   below 700 Hz guts intelligibility entirely. The design explicitly
//   allows "failing/garbled" at d_max (vibe-guardian condition) but asks
//   for a floor so voice never becomes useless noise.
//
// Returns:
//   Ok(fc_hz)
//   Err(InvalidDistance)  if d < 0 or non-finite.
//   Err(InvalidOcclusion) if occ outside [0,1] or non-finite.
// ---------------------------------------------------------------------------
Result<float, SpatializerError>
distance_lpf_cutoff(float d, float occ, const VoiceRangeParams& r);

// ---------------------------------------------------------------------------
// rear_cue_gain_db — returns the additional gain in dB applied to a source
// in the rear hemisphere. -3 dB — enough to signal "behind you" without
// making the source inaudible. See Blauert §4.3 for the psychoacoustic basis
// (front-back confusion resolved partly by spectral shaping, partly by
// slight amplitude difference from pinna shadowing).
// ---------------------------------------------------------------------------
constexpr float rear_cue_gain_db() noexcept { return -3.0f; }

// ---------------------------------------------------------------------------
// rear_lpf_cutoff_hz — the LPF cutoff applied to rear sources as a crude
// pinna-shadow cue. 1 kHz: rear sources lose most of their "presence" band
// (2-4 kHz) and the ear-brain reads them as behind. This is stacked with
// the distance LPF — the filter chain takes the min of the two cutoffs
// per frame, since two serial LPFs at cutoffs fa ≤ fb are dominated by fa.
// ---------------------------------------------------------------------------
constexpr float rear_lpf_cutoff_hz() noexcept { return 1000.0f; }

} // namespace odyssey::audio::voice::dsp
