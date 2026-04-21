#pragma once
//
// ducker.h — pure sidechain envelope follower + EQ-carve decision for the
// proximity-voice ducking system.
//
// "Music should bow slightly when a human speaks. Never the other way around."
// — design doc §7.
//
// Responsibilities:
//   - Track "voice is active" state over time with asymmetric attack/release.
//   - Produce target gains in dB for the music / SFX / ambient buses.
//   - Produce a boolean "carve engaged" flag that the music bus uses to run
//     (or bypass) a peaking EQ at 3 kHz / Q=1 / -4 dB per Marty KB §11.
//   - Produce a "target gain after silence" helper expressing the
//     2-second full-restore curve required by the marty-full-restore council
//     condition.
//
// Pure, stateful-through-caller-owned-struct. No I/O.
//

#include "core/result.h"

#include <cstdint>

namespace odyssey::audio::voice::dsp {

// ---------------------------------------------------------------------------
// DuckerParams — tunable trims (Marty's "tunable sidechain" council condition).
// All gains in dB; music_voice_priority scales the duck depth so combat
// music can resist ducking more than ambient.
// ---------------------------------------------------------------------------
struct DuckerParams {
    // Per-bus duck targets applied when voice is fully active.
    float music_duck_db   = -6.0f; // whole-bus duck
    float sfx_duck_db     = -3.0f;
    float ambient_duck_db = -4.0f;

    // Peaking EQ carve parameters on music bus (stacked with whole-bus duck).
    // Marty KB §11: voice-as-soloist sits in 2-4 kHz presence band; carving
    // a -4 dB hole at 3 kHz / Q=1 opens ensemble space for voice without
    // dropping the whole mix.
    float carve_freq_hz = 3000.0f;
    float carve_q       = 1.0f;
    float carve_gain_db = -4.0f;

    // Envelope time constants (design doc §7.1 — asymmetric on purpose).
    float attack_ms  = 50.0f;  // fast in — don't clip the first syllable
    float release_ms = 400.0f; // slow out — don't pump between words

    // Full-restore window (marty-full-restore council condition).
    float restore_seconds = 2.0f;

    // Priority scale: 1.0 = full duck, 0.0 = no duck.
    // Per-music-state resistor (combat cue might use 0.5; ambient uses 1.0).
    float music_voice_priority = 1.0f;
};

// ---------------------------------------------------------------------------
// DuckerState — caller-owned cross-frame memory.
//   env: current envelope value in [0, 1]; 0 = no voice, 1 = full duck.
//   time_since_last_voice_s: monotonic seconds since last frame where voice
//   was active. Used to drive the 2-second full-restore curve.
// ---------------------------------------------------------------------------
struct DuckerState {
    float env = 0.0f;
    float time_since_last_voice_s = 10.0f; // start "long silent" so no duck on boot
};

// ---------------------------------------------------------------------------
// DuckerEnvelope — one frame's worth of computed targets, ready to be
// consumed by the voice_bus / music_director / mixer.
//
// All dB values are the *multiplicative correction* to apply: 0.0 dB means
// "no change", -6.0 means "attenuate 6 dB".
// ---------------------------------------------------------------------------
struct DuckerEnvelope {
    float music_gain_db   = 0.0f;
    float sfx_gain_db     = 0.0f;
    float ambient_gain_db = 0.0f;
    bool  carve_engaged   = false; // music bus should run peaking EQ this frame
    float env_value       = 0.0f;  // 0..1, for overlay/debug
};

enum class DuckerError : uint32_t {
    InvalidDt     = 1, // dt <= 0 or non-finite
    InvalidParams = 2, // attack/release <= 0, etc.
};

// ---------------------------------------------------------------------------
// tick — advance the envelope by dt seconds, producing this frame's targets.
//
// Derivation.
//   Exponential approach to a target with time-constant τ:
//       env[n+1] = env[n] + α · (target - env[n])
//       α = 1 - exp(-dt / τ)
//   This is the discrete form of dy/dt = (target - y) / τ. For small dt/τ
//   it linearizes to α ≈ dt/τ; for our frame sizes (dt=20 ms, τ=50-400 ms)
//   the exp is the correct one to avoid overshoot at frame-rate boundaries.
//
//   We use separate α for attack vs release so duck-in is fast (don't clip
//   the first syllable) and duck-out is slow (don't pump between words).
//
// Bus-specific gains: once the envelope is known, each bus's dB target is
//   bus_db = env · bus_duck_db · music_voice_priority    (for music only)
// For sfx/ambient the priority scalar is not applied — they always duck
// the full amount when voice is active (the priority is a *music-composition*
// control per the "music_voice_priority per music state" addition).
//
// Carve engagement: we only engage the EQ carve when env > 0.05 so a single
// clicked mic doesn't toggle the filter; hysteresis is built into the
// envelope itself via the asymmetric time constants.
//
// Returns:
//   Ok(DuckerEnvelope) with targets for this frame.
//   Err(InvalidDt)     if dt is <= 0 or non-finite.
//   Err(InvalidParams) if attack/release/restore times are non-positive.
// ---------------------------------------------------------------------------
Result<DuckerEnvelope, DuckerError>
tick(DuckerState& state, bool voice_active, float dt_seconds, const DuckerParams& params);

// ---------------------------------------------------------------------------
// target_gain_after_silence — pure helper for the marty-full-restore
// council condition. Returns the target music-bus dB as a function of
// seconds since voice last went inactive.
//
// Design: smooth cos-shaped curve from the ducked value back to 0 dB over
// `restore_seconds`. A linear ramp would sound mechanical at long time
// scales; cos² matches the distance-attenuation taper's aesthetic.
//
//   let t ∈ [0, restore_seconds]
//   u = clamp(t / restore_seconds, 0, 1)
//   blend = sin²(u · π/2)   (0 at u=0, 1 at u=1, smooth)
//   gain_db = (1 - blend) · music_duck_db_at_voice_end
//
// Notes:
//   - Expresses *target*, not envelope output. The envelope's release
//     time-constant already smooths approach; this function is a higher-
//     level "after 2 s we MUST be fully restored" guarantee that the
//     envelope time-constants then pursue.
//   - Time-constant-only release (400 ms) settles to within ~1 % in ~2 s
//     organically; this function codifies that as a design invariant and
//     the test locks it down.
// ---------------------------------------------------------------------------
float target_gain_after_silence(float time_since_last_voice_s,
                                const DuckerParams& params) noexcept;

} // namespace odyssey::audio::voice::dsp
