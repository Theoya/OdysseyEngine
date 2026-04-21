//
// ducker.cpp — sidechain envelope follower and EQ carve gating.
//

#include "audio/voice/dsp/ducker.h"

#include <algorithm>
#include <cmath>

namespace odyssey::audio::voice::dsp {

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double HALF_PI = 1.57079632679489661923;

inline bool finite(float x) noexcept { return std::isfinite(x); }

bool params_ok(const DuckerParams& p) noexcept {
    if (!finite(p.music_duck_db) || !finite(p.sfx_duck_db) || !finite(p.ambient_duck_db)) return false;
    if (!finite(p.attack_ms)  || p.attack_ms  <= 0.0f) return false;
    if (!finite(p.release_ms) || p.release_ms <= 0.0f) return false;
    if (!finite(p.restore_seconds) || p.restore_seconds <= 0.0f) return false;
    if (!finite(p.music_voice_priority) ||
        p.music_voice_priority < 0.0f || p.music_voice_priority > 1.0f) return false;
    if (!finite(p.carve_freq_hz) || p.carve_freq_hz <= 0.0f) return false;
    if (!finite(p.carve_q) || p.carve_q <= 0.0f) return false;
    if (!finite(p.carve_gain_db)) return false;
    return true;
}

// Discrete one-pole coefficient α = 1 - exp(-dt / τ).
inline float alpha_from_tau_ms(float dt_seconds, float tau_ms) noexcept {
    const double tau_s = static_cast<double>(tau_ms) * 1e-3;
    return static_cast<float>(1.0 - std::exp(-static_cast<double>(dt_seconds) / tau_s));
}

} // namespace

// ---------------------------------------------------------------------------
// tick
// ---------------------------------------------------------------------------
Result<DuckerEnvelope, DuckerError>
tick(DuckerState& state, bool voice_active, float dt_seconds, const DuckerParams& params) {
    if (!finite(dt_seconds) || dt_seconds <= 0.0f)
        return Result<DuckerEnvelope, DuckerError>::err(DuckerError::InvalidDt);
    if (!params_ok(params))
        return Result<DuckerEnvelope, DuckerError>::err(DuckerError::InvalidParams);

    // Target envelope: 1.0 while voice is active, 0.0 otherwise.
    const float target = voice_active ? 1.0f : 0.0f;

    // Asymmetric α — attack when envelope rising toward 1, release when falling.
    const float tau_ms = (target > state.env) ? params.attack_ms : params.release_ms;
    const float alpha  = alpha_from_tau_ms(dt_seconds, tau_ms);

    // One-pole update.
    state.env += alpha * (target - state.env);
    state.env = std::clamp(state.env, 0.0f, 1.0f);

    // Time-since-voice bookkeeping (for target_gain_after_silence callers).
    if (voice_active) {
        state.time_since_last_voice_s = 0.0f;
    } else {
        state.time_since_last_voice_s += dt_seconds;
    }

    // Bus gains: env * duck_db, with the music priority scalar applied only
    // to the music bus.
    DuckerEnvelope out;
    out.env_value       = state.env;
    out.music_gain_db   = state.env * params.music_duck_db * params.music_voice_priority;
    out.sfx_gain_db     = state.env * params.sfx_duck_db;
    out.ambient_gain_db = state.env * params.ambient_duck_db;

    // Carve engagement: engage once env rises above 5% and stay engaged until
    // it falls below 1% (bit of hysteresis to prevent chatter on weak
    // envelopes from a hot mic). The comparison uses env directly — which
    // already has the attack/release smoothing baked in.
    //
    // We don't need separate state for this because the envelope itself is
    // the hysteresis: it can't cross 0.05 → 0.01 instantaneously at realistic
    // time constants.
    if (state.env > 0.05f) out.carve_engaged = true;
    else if (state.env < 0.01f) out.carve_engaged = false;
    else out.carve_engaged = (state.env > 0.03f); // dead zone: previous-ish behavior

    return Result<DuckerEnvelope, DuckerError>::ok(out);
}

// ---------------------------------------------------------------------------
// target_gain_after_silence
// ---------------------------------------------------------------------------
float target_gain_after_silence(float time_since_last_voice_s,
                                const DuckerParams& params) noexcept {
    // Guard — NaN / negative / infinite inputs collapse to the fully-ducked
    // value (caller can't sensibly consume NaN here).
    if (!finite(time_since_last_voice_s) || time_since_last_voice_s < 0.0f) {
        return params.music_duck_db * params.music_voice_priority;
    }

    // u = clamp(t / restore_seconds, 0, 1); past the window we're fully restored.
    const float t = time_since_last_voice_s;
    const float R = params.restore_seconds;
    const float u = std::clamp(t / R, 0.0f, 1.0f);

    // blend = sin²(u · π/2): 0 at u=0 → 1 at u=1.
    const float s = static_cast<float>(std::sin(static_cast<double>(u) * HALF_PI));
    const float blend = s * s;

    // (1 - blend) * ducked_db — ducked at t=0, restored at t >= restore_seconds.
    const float ducked_db = params.music_duck_db * params.music_voice_priority;
    return (1.0f - blend) * ducked_db;
}

} // namespace odyssey::audio::voice::dsp
