#pragma once
//
// sidechain_ducker.h — exponential envelope for ducking MusicBus level during voice speech.
//
// Derivation (condition 11, condition 2):
//   Exponential decay: y[n] = y[n-1] + α(target - y[n-1])
//   α = 1 - exp(-dt/τ)  where τ=2.0s is the time-constant.
//
//   Continuous-time first-order ODE: dy/dt = (target - y) / τ
//   Discrete approximation: y[n] = y[n-1] + ∫[n-1,n] (target - y(t)) / τ dt
//   ≈ y[n-1] + (target - y[n-1]) ∫[0,dt] exp(-t/τ) / τ dt
//   = y[n-1] + (target - y[n-1]) (1 - exp(-dt/τ))
//
// Stinger priority (condition 2):
//   While a stinger voice is live on MUSIC_BUS, ducker gain target is
//   max(current, -3dB) = max(current_linear, 0.7079) so it can attenuate
//   slightly but not smash the hit. The 3kHz carve (biquad EQ) is BYPASSED
//   entirely for the stinger's duration. Rationale: stinger is an accent;
//   smashing it destroys the point. EQ bypass preserves clarity.
//
// Silence as primitive (condition 6): When no voice is active, target is 0
//   (no floor; output may be exactly 0.0f).
//

#include <cstdint>
#include <cmath>

namespace odyssey::audio::music::detail {

// Gain smoothing state (caller-owned per frame, updated by tick()).
struct SidechainDuckerState {
    float current_gain_linear = 1.0f; // Current smooth gain [0, 1].
};

// Ducker parameters (tunable per scene).
struct SidechainDuckerParams {
    float gain_target_when_active_db = -6.0f;  // dB reduction during speech.
    float gain_target_idle_db = 0.0f;           // dB at rest (unity).
    float time_constant_s = 2.0f;               // τ = 2.0s per Marty KB §11.
    float stinger_priority_floor_db = -3.0f;   // max(current, -3dB) during stinger.
};

// Tick the ducker state: smooth toward target, optionally with stinger override.
// Pure function.
inline void tick_sidechain_ducker(
    SidechainDuckerState& state,
    bool any_voice_active,
    bool stinger_voice_active,
    float dt_seconds,
    const SidechainDuckerParams& params) noexcept {

    // Determine target gain in dB.
    float target_db = any_voice_active ? params.gain_target_when_active_db
                                       : params.gain_target_idle_db;

    // Stinger priority: floor at -3dB to preserve the hit.
    if (stinger_voice_active) {
        target_db = std::fmax(target_db, params.stinger_priority_floor_db);
    }

    // Convert to linear.
    float target_linear = std::pow(10.0f, target_db / 20.0f);

    // Exponential smoothing: α = 1 - exp(-dt/τ)
    float alpha = 1.0f - std::exp(-dt_seconds / params.time_constant_s);

    // Update: y[n] = y[n-1] + α(target - y[n-1])
    state.current_gain_linear = state.current_gain_linear
                              + alpha * (target_linear - state.current_gain_linear);
}

} // namespace odyssey::audio::music::detail
