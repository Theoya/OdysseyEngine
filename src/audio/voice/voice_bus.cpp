// voice_bus.cpp — implementation of the pure listener-centric voice mix.
//
// Design references:
//   - docs/decisions/2026-04-20-proximity-voice-chat.md (council conditions)
//   - docs/design/proximity_chat_audio.md §6-7
//   - Marty KB §11 "Voice as ensemble"

#include "audio/voice/voice_bus.h"

#include <cmath>
#include <limits>

namespace odyssey::audio::voice {

namespace {

bool is_finite_pose(const dsp::ListenerPose& p) noexcept {
    auto ok = [](float v) { return std::isfinite(v); };
    return ok(p.position.x) && ok(p.position.y) && ok(p.position.z)
        && ok(p.forward.x)  && ok(p.forward.y)  && ok(p.forward.z)
        && ok(p.right.x)    && ok(p.right.y)    && ok(p.right.z)
        && ok(p.up.x)       && ok(p.up.y)       && ok(p.up.z);
}

// Serial LPF chain collapses to the minimum cutoff to a first approximation
// (two one-poles at fa <= fb are dominated by fa up to a few dB past fa). We
// stack distance LPF + rear-cue LPF as the min.
inline float stack_lpf(float a, float b) noexcept {
    return std::fmin(a, b);
}

} // namespace

Result<ListenerMix, VoiceBusError>
preview_for_listener(const dsp::ListenerPose&                  listener,
                     const std::vector<SpeakerSnapshot>&       speakers,
                     const dsp::VoiceRangeParams&              range,
                     dsp::DuckerState&                         ducker_state,
                     const dsp::DuckerParams&                  ducker_params,
                     float                                     dt_seconds) {
    using R = Result<ListenerMix, VoiceBusError>;

    if (!is_finite_pose(listener)) {
        return R::err(VoiceBusError::InvalidListener);
    }
    if (speakers.size() > kMaxSpeakers) {
        return R::err(VoiceBusError::TooManySpeakers);
    }

    ListenerMix mix;
    mix.sources.reserve(speakers.size());

    bool any_active_voice = false;

    for (const auto& s : speakers) {
        MixedSource out{};
        out.speaker_entity_id = s.speaker_entity_id;

        if (s.muted_by_listener) {
            out.active = false;
            mix.speakers_dropped++;
            mix.sources.push_back(out);
            continue;
        }

        // Distance and azimuth.
        const float dx = s.pose.position.x - listener.position.x;
        const float dy = s.pose.position.y - listener.position.y;
        const float dz = s.pose.position.z - listener.position.z;
        const float d  = std::sqrt(dx*dx + dy*dy + dz*dz);
        out.distance_m = d;

        // Speaker-specific d_max overrides the default (voice_range stat).
        dsp::VoiceRangeParams per_speaker = range;
        per_speaker.d_max = s.voice_range_m;
        // Guard against degenerate scene data — if a prefab set voice_range
        // to something invalid, fall back to default d_max rather than
        // dropping the speaker entirely (graceful degradation).
        if (!(per_speaker.d_max > per_speaker.d_min) ||
            !std::isfinite(per_speaker.d_max)) {
            per_speaker.d_max = range.d_max;
        }
        // Ensure taper fits — pure math invariant for the spatializer.
        if (per_speaker.taper_width > per_speaker.d_max - per_speaker.d_min) {
            per_speaker.taper_width = per_speaker.d_max - per_speaker.d_min;
        }

        // Distance attenuation.
        auto att_r = dsp::distance_attenuation(d, per_speaker);
        if (att_r.is_err()) {
            return R::err(VoiceBusError::InvalidParams);
        }
        out.attenuation_linear = att_r.value();
        if (out.attenuation_linear <= 0.0f) {
            out.active = false;
            mix.speakers_dropped++;
            mix.sources.push_back(out);
            continue;
        }

        // Azimuth: angle between listener.forward and (speaker - listener) in
        // the listener's horizontal plane. We project to the listener's
        // forward/right plane; positive azimuth = right.
        const float len = d > 1e-6f ? d : 1e-6f;
        const float nx = dx / len;
        const float ny = dy / len;
        const float nz = dz / len;
        const float fwd = nx*listener.forward.x + ny*listener.forward.y + nz*listener.forward.z;
        const float rgt = nx*listener.right.x   + ny*listener.right.y   + nz*listener.right.z;
        // θ = atan2(right, forward). Front-center = 0, +π/2 = hard right,
        // |θ|>π/2 = rear hemisphere.
        float theta = std::atan2(rgt, fwd);
        out.rear_hemisphere = std::fabs(theta) > 1.57079632679f; // π/2

        // Fold rear θ into front hemisphere for the pan computation (see
        // spatializer.h rationale).
        float theta_pan = theta;
        if (out.rear_hemisphere) {
            theta_pan = (theta > 0.0f) ? (3.14159265359f - theta) :
                                         (-3.14159265359f - theta);
        }
        // Clamp into [-π/2, +π/2] after fold (numerical guard).
        if (theta_pan > 1.57079632679f)  theta_pan =  1.57079632679f;
        if (theta_pan < -1.57079632679f) theta_pan = -1.57079632679f;

        auto pan_r = dsp::equal_power_pan(theta_pan);
        if (pan_r.is_err()) {
            return R::err(VoiceBusError::InvalidParams);
        }
        const dsp::PanPair pan = pan_r.value();
        out.pan_L = pan.L;
        out.pan_R = pan.R;

        // Rear-hemisphere attenuation is stacked as an additional linear
        // scale (−3 dB ≈ 0.7079). The LPF cue is stacked with the distance
        // LPF below.
        if (out.rear_hemisphere) {
            const float rear_lin = std::pow(10.0f, dsp::rear_cue_gain_db() / 20.0f);
            out.attenuation_linear *= rear_lin;
        }

        // Distance LPF.
        auto lpf_r = dsp::distance_lpf_cutoff(d, s.occlusion_0_to_1, per_speaker);
        if (lpf_r.is_err()) {
            return R::err(VoiceBusError::InvalidParams);
        }
        float cutoff = lpf_r.value();
        if (out.rear_hemisphere) {
            cutoff = stack_lpf(cutoff, dsp::rear_lpf_cutoff_hz());
        }
        out.lpf_cutoff_hz = cutoff;

        out.active = true;
        any_active_voice = true;
        mix.speakers_in_range++;
        mix.sources.push_back(out);
    }

    // Drive the ducker envelope from "is any speaker audible to this listener"
    // rather than "is anyone transmitting globally", so a listener too far
    // from a speaker does not hear music duck uncomfortably.
    auto duck_r = dsp::tick(ducker_state, any_active_voice, dt_seconds, ducker_params);
    if (duck_r.is_err()) {
        return R::err(VoiceBusError::InvalidParams);
    }
    mix.duck_envelope = duck_r.value();

    return R::ok(std::move(mix));
}

} // namespace odyssey::audio::voice
