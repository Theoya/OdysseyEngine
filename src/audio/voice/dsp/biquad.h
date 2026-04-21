#pragma once
//
// biquad.h — pure RBJ Audio EQ Cookbook biquad filter.
//
// This header defines a single-section transposed-direct-form II biquad IIR
// filter plus a pair of coefficient factories (peaking EQ and low-pass).
// It is strictly pure: no I/O, no globals, no allocation. The only state that
// crosses samples is the caller-owned BiquadState struct.
//
// References:
//   - Robert Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter
//     coefficients" (https://www.w3.org/TR/audio-eq-cookbook/). Cited inline
//     as "RBJ §N".
//   - Zölzer, "DAFX: Digital Audio Effects", 2nd ed., ch. 2 (IIR filters).
//
// Engine mandate #1 (pure functions): coefficient factories and process()
// are pure; they return Result<T,E> on invalid input rather than throwing.
// Mandate #3 (first-principles math): every coefficient line carries a
// derivation comment referencing the cookbook section.
//

#include <cstddef>
#include <cstdint>
#include "core/result.h"

namespace odyssey::audio::voice::dsp {

// ---------------------------------------------------------------------------
// Error type — why a coefficient request or process step was rejected.
// ---------------------------------------------------------------------------
enum class FilterError : uint32_t {
    InvalidSampleRate = 1, // fs <= 0 or non-finite
    InvalidFrequency  = 2, // f0 <= 0, >= Nyquist, or non-finite
    InvalidQ          = 3, // Q <= 0 or non-finite
    InvalidGain       = 4, // gain_db non-finite
    NanInput          = 5, // process() received NaN/Inf sample
};

// ---------------------------------------------------------------------------
// BiquadCoeffs — the six frequency-domain coefficients of a second-order
// biquad, normalized by a0 so the transfer function is:
//
//            b0 + b1 z^-1 + b2 z^-2
//     H(z) = ----------------------
//             1 + a1 z^-1 + a2 z^-2
//
// (RBJ §1 normalization — divide every numerator + denominator coefficient
// by the a0 the cookbook produces, so we can store five numbers and assume
// a0 == 1 in the difference equation.)
// ---------------------------------------------------------------------------
struct BiquadCoeffs {
    float b0{1.0f};
    float b1{0.0f};
    float b2{0.0f};
    float a1{0.0f}; // note: a0 is always 1.0 after normalization; not stored.
    float a2{0.0f};
};

// ---------------------------------------------------------------------------
// BiquadState — per-filter running history. Transposed direct form II carries
// two state variables (z1, z2) rather than two input + two output delays,
// which halves memory and is numerically better for fixed or normalized
// floating point (Zölzer §2.4.3). One BiquadState per filter *per channel*.
// ---------------------------------------------------------------------------
struct BiquadState {
    float z1{0.0f};
    float z2{0.0f};
};

// ---------------------------------------------------------------------------
// peaking_eq — RBJ §8.5 "peakingEQ". Boosts or cuts a single band centered
// on f0 with bandwidth controlled by Q. Gain is specified in dB.
//
// Used by the proximity-voice ducker to carve a -4 dB hole at 3 kHz on the
// music bus when a teammate speaks (Marty KB §11 — voice as a soloist in the
// 2-4 kHz presence region).
//
// Inputs:
//   fs       — sample rate in Hz (48000 for our pipeline)
//   f0       — center frequency in Hz, must satisfy 0 < f0 < fs/2
//   Q        — quality factor, must be > 0 (higher = narrower notch/peak)
//   gain_db  — peak gain in dB (positive = boost, negative = cut)
// ---------------------------------------------------------------------------
Result<BiquadCoeffs, FilterError> peaking_eq(float fs, float f0, float Q, float gain_db);

// ---------------------------------------------------------------------------
// lowpass — RBJ §8.3 "LPF". Standard second-order Butterworth-style low-pass
// when Q = 1/sqrt(2) (~0.7071); higher Q creates a resonant peak at f0.
//
// Used inside the spatializer for the distance-driven LPF (air absorption
// plus a rear-hemisphere 1 kHz cue).
// ---------------------------------------------------------------------------
Result<BiquadCoeffs, FilterError> lowpass(float fs, float f0, float Q);

// ---------------------------------------------------------------------------
// process — single-sample tick. Transposed direct form II difference:
//     y[n] = b0 * x[n] + z1
//     z1   = b1 * x[n] - a1 * y[n] + z2
//     z2   = b2 * x[n] - a2 * y[n]
// (Zölzer §2.4.3 — transposed form guarantees the output depends only on
// current input and prior state, avoiding the extra multiply vs direct form.)
//
// Pure aside from mutating the caller-owned BiquadState.
// Returns NanInput if x is not finite — refuses to taint the state with NaN.
// ---------------------------------------------------------------------------
Result<float, FilterError> process(const BiquadCoeffs& c, BiquadState& s, float x);

// ---------------------------------------------------------------------------
// process_unchecked — hot-path variant skipping the NaN guard. Use only when
// the caller has already validated the sample (e.g. the mixer's inner loop).
// ---------------------------------------------------------------------------
float process_unchecked(const BiquadCoeffs& c, BiquadState& s, float x) noexcept;

} // namespace odyssey::audio::voice::dsp
