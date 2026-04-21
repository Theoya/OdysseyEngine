// test_voice_dsp.cpp — consolidated unit tests for the proximity-voice DSP
// primitives (biquad, spatializer, vad, ducker).
//
// Council condition (architect): success + failure per Result<T,E> entry point
// — NaN guards, invalid sample rate, out-of-range listener, EQ coefficients.
// Council condition (marty): full-restore at 2 s; attack/release timing.

#include "audio/voice/dsp/biquad.h"
#include "audio/voice/dsp/spatializer.h"
#include "audio/voice/dsp/vad.h"
#include "audio/voice/dsp/ducker.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

using namespace odyssey::audio::voice::dsp;

// =============================================================================
// Biquad
// =============================================================================

TEST(Biquad, PeakingEqHappyPath) {
    auto r = peaking_eq(48000.0f, 3000.0f, 1.0f, -4.0f);
    ASSERT_TRUE(r.is_ok());
    const auto c = r.value();
    // b0 > 0 and finite — sanity.
    EXPECT_TRUE(std::isfinite(c.b0));
    EXPECT_TRUE(std::isfinite(c.b1));
    EXPECT_TRUE(std::isfinite(c.b2));
    EXPECT_TRUE(std::isfinite(c.a1));
    EXPECT_TRUE(std::isfinite(c.a2));
}

TEST(Biquad, PeakingEqInvalidSampleRate) {
    auto r = peaking_eq(-1.0f, 3000.0f, 1.0f, -4.0f);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), FilterError::InvalidSampleRate);
}

TEST(Biquad, PeakingEqInvalidQ) {
    auto r = peaking_eq(48000.0f, 3000.0f, 0.0f, -4.0f);
    EXPECT_TRUE(r.is_err());
    auto r2 = peaking_eq(48000.0f, 3000.0f, -1.0f, -4.0f);
    EXPECT_TRUE(r2.is_err());
}

TEST(Biquad, PeakingEqFrequencyAboveNyquist) {
    auto r = peaking_eq(48000.0f, 30000.0f, 1.0f, -4.0f);
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), FilterError::InvalidFrequency);
}

TEST(Biquad, LowpassHappyPath) {
    auto r = lowpass(48000.0f, 8000.0f, 0.7071f);
    ASSERT_TRUE(r.is_ok());
    EXPECT_TRUE(std::isfinite(r.value().b0));
}

TEST(Biquad, ProcessNaNGuardRefuses) {
    auto coeffs = lowpass(48000.0f, 8000.0f, 0.7071f).value();
    BiquadState s{};
    auto r = process(coeffs, s, std::numeric_limits<float>::quiet_NaN());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), FilterError::NanInput);
    // State should not be tainted.
    EXPECT_EQ(s.z1, 0.0f);
    EXPECT_EQ(s.z2, 0.0f);
}

TEST(Biquad, LowpassAttenuatesHighFrequency) {
    // Run a 15 kHz tone through an 8 kHz LPF and check the steady-state RMS
    // is lower than the input RMS.
    auto c = lowpass(48000.0f, 8000.0f, 0.7071f).value();
    BiquadState s{};
    double in_sq = 0.0, out_sq = 0.0;
    constexpr int N = 2048;
    constexpr float two_pi = 6.2831853f;
    for (int n = 0; n < N; ++n) {
        float x = std::sin(two_pi * 15000.0f * static_cast<float>(n) / 48000.0f);
        float y = process_unchecked(c, s, x);
        if (n > N / 2) { // skip transient
            in_sq  += double(x) * x;
            out_sq += double(y) * y;
        }
    }
    const double in_rms  = std::sqrt(in_sq  / (N / 2));
    const double out_rms = std::sqrt(out_sq / (N / 2));
    EXPECT_LT(out_rms, in_rms * 0.7) << "15 kHz tone should be attenuated past 8 kHz cutoff";
}

// =============================================================================
// Spatializer
// =============================================================================

TEST(Spatializer, DistanceAttenuationInsideMinIsUnity) {
    VoiceRangeParams r{};
    auto v = distance_attenuation(0.5f, r);
    ASSERT_TRUE(v.is_ok());
    EXPECT_FLOAT_EQ(v.value(), 1.0f);
}

TEST(Spatializer, DistanceAttenuationAtDMinIsUnity) {
    VoiceRangeParams r{};
    auto v = distance_attenuation(1.0f, r);
    ASSERT_TRUE(v.is_ok());
    EXPECT_FLOAT_EQ(v.value(), 1.0f);
}

TEST(Spatializer, DistanceAttenuationBeyondDMaxIsZero) {
    VoiceRangeParams r{};
    auto v = distance_attenuation(30.0f, r);
    ASSERT_TRUE(v.is_ok());
    EXPECT_FLOAT_EQ(v.value(), 0.0f);
}

TEST(Spatializer, DistanceAttenuationMonotoneDecreasing) {
    VoiceRangeParams r{};
    float last = 1.0f;
    for (float d = 1.0f; d < 24.0f; d += 1.0f) {
        auto v = distance_attenuation(d, r);
        ASSERT_TRUE(v.is_ok());
        EXPECT_LE(v.value(), last + 1e-5f) << "at d=" << d;
        last = v.value();
    }
}

TEST(Spatializer, DistanceAttenuationNegativeIsError) {
    VoiceRangeParams r{};
    auto v = distance_attenuation(-1.0f, r);
    ASSERT_TRUE(v.is_err());
    EXPECT_EQ(v.error(), SpatializerError::InvalidDistance);
}

TEST(Spatializer, DistanceAttenuationNaNIsError) {
    VoiceRangeParams r{};
    auto v = distance_attenuation(std::numeric_limits<float>::quiet_NaN(), r);
    EXPECT_TRUE(v.is_err());
}

TEST(Spatializer, EqualPowerPanCenter) {
    auto r = equal_power_pan(0.0f);
    ASSERT_TRUE(r.is_ok());
    const auto p = r.value();
    EXPECT_NEAR(p.L, 0.7071068f, 1e-4f);
    EXPECT_NEAR(p.R, 0.7071068f, 1e-4f);
}

TEST(Spatializer, EqualPowerPanHardRight) {
    auto r = equal_power_pan(1.57079632679f); // +π/2
    ASSERT_TRUE(r.is_ok());
    const auto p = r.value();
    EXPECT_NEAR(p.L, 0.0f, 1e-4f);
    EXPECT_NEAR(p.R, 1.0f, 1e-4f);
}

TEST(Spatializer, EqualPowerPanHardLeft) {
    auto r = equal_power_pan(-1.57079632679f); // -π/2
    ASSERT_TRUE(r.is_ok());
    const auto p = r.value();
    EXPECT_NEAR(p.L, 1.0f, 1e-4f);
    EXPECT_NEAR(p.R, 0.0f, 1e-4f);
}

TEST(Spatializer, EqualPowerPanLawInvariant) {
    // L² + R² == 1 across the full arc.
    for (float theta = -1.57f; theta <= 1.57f; theta += 0.1f) {
        auto r = equal_power_pan(theta);
        ASSERT_TRUE(r.is_ok());
        const auto p = r.value();
        const float energy = p.L*p.L + p.R*p.R;
        EXPECT_NEAR(energy, 1.0f, 1e-4f) << "theta=" << theta;
    }
}

TEST(Spatializer, DistanceLpfCutoffDecreasesWithDistance) {
    VoiceRangeParams r{};
    auto c1 = distance_lpf_cutoff(1.0f,  0.0f, r);
    auto c2 = distance_lpf_cutoff(12.0f, 0.0f, r);
    auto c3 = distance_lpf_cutoff(24.0f, 0.0f, r);
    ASSERT_TRUE(c1.is_ok() && c2.is_ok() && c3.is_ok());
    EXPECT_GT(c1.value(), c2.value());
    EXPECT_GT(c2.value(), c3.value());
    EXPECT_GE(c3.value(), 700.0f); // floor
}

TEST(Spatializer, DistanceLpfCutoffOcclusionLowers) {
    VoiceRangeParams r{};
    auto c_clear   = distance_lpf_cutoff(12.0f, 0.0f, r).value();
    auto c_occluded = distance_lpf_cutoff(12.0f, 1.0f, r).value();
    EXPECT_GT(c_clear, c_occluded);
}

// =============================================================================
// VAD
// =============================================================================

TEST(Vad, SilentFrameInactive) {
    std::vector<float> frame(960, 0.0f);
    VadParams p{}; VadState s{};
    auto r = analyze(frame, p, s);
    ASSERT_TRUE(r.is_ok());
    EXPECT_FALSE(r.value().active);
}

TEST(Vad, LoudSineActive) {
    std::vector<float> frame(960);
    for (int i = 0; i < 960; ++i) {
        frame[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f * i / 48000.0f);
    }
    VadParams p{}; VadState s{};
    auto r = analyze(frame, p, s);
    ASSERT_TRUE(r.is_ok());
    EXPECT_TRUE(r.value().active);
    EXPECT_GT(r.value().rms_dbfs, -20.0f);
}

TEST(Vad, EmptyFrameIsError) {
    std::vector<float> empty;
    VadParams p{}; VadState s{};
    auto r = analyze(empty, p, s);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), VadError::EmptyFrame);
}

TEST(Vad, NonFiniteSampleIsError) {
    std::vector<float> frame(960, 0.0f);
    frame[10] = std::numeric_limits<float>::quiet_NaN();
    VadParams p{}; VadState s{};
    auto r = analyze(frame, p, s);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), VadError::NonFiniteSample);
}

TEST(Vad, Hysteresis) {
    // Loud frame → active. Silent frame immediately after → still active
    // during hang window. After enough hang frames → inactive.
    std::vector<float> loud(960);
    for (int i = 0; i < 960; ++i) {
        loud[i] = 0.5f * std::sin(2.0f * 3.14159f * 1000.0f * i / 48000.0f);
    }
    std::vector<float> silent(960, 0.0f);

    VadParams p{}; VadState s{};
    auto a1 = analyze(loud, p, s); ASSERT_TRUE(a1.is_ok());
    EXPECT_TRUE(a1.value().active);

    auto a2 = analyze(silent, p, s); ASSERT_TRUE(a2.is_ok());
    EXPECT_TRUE(a2.value().active) << "should still be active during hang";

    // Hang is 200 ms / 20 ms = 10 frames. After 11 silent frames, gate closes.
    for (int i = 0; i < 11; ++i) {
        auto a = analyze(silent, p, s); ASSERT_TRUE(a.is_ok());
    }
    auto after = analyze(silent, p, s); ASSERT_TRUE(after.is_ok());
    EXPECT_FALSE(after.value().active);
}

// =============================================================================
// Ducker
// =============================================================================

TEST(Ducker, VoiceActivePullsMusicDown) {
    DuckerState st{};
    DuckerParams p{};
    // Run enough frames to reach near-steady-state (attack is 50 ms; 10 frames
    // of 20 ms = 200 ms, plenty of settling).
    DuckerEnvelope out{};
    for (int i = 0; i < 20; ++i) {
        auto r = tick(st, true, 0.02f, p);
        ASSERT_TRUE(r.is_ok());
        out = r.value();
    }
    EXPECT_LT(out.music_gain_db, -5.0f); // within 1 dB of -6 dB target
    EXPECT_TRUE(out.carve_engaged);
}

TEST(Ducker, VoiceInactiveRestoresToZero) {
    DuckerState st{};
    DuckerParams p{};
    // Duck first.
    for (int i = 0; i < 20; ++i) {
        (void)tick(st, true, 0.02f, p);
    }
    // Then release.
    DuckerEnvelope out{};
    for (int i = 0; i < 100; ++i) { // 2.0 seconds
        auto r = tick(st, false, 0.02f, p);
        ASSERT_TRUE(r.is_ok());
        out = r.value();
    }
    EXPECT_GT(out.music_gain_db, -0.5f) << "should have fully restored by 2s";
}

TEST(Ducker, InvalidDt) {
    DuckerState st{}; DuckerParams p{};
    auto r = tick(st, true, -0.1f, p);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), DuckerError::InvalidDt);
}

TEST(Ducker, TargetGainAfterSilenceCurve) {
    DuckerParams p{};
    // t=0 → fully ducked (near music_duck_db).
    const float at_zero = target_gain_after_silence(0.0f, p);
    EXPECT_LT(at_zero, -4.0f);
    // t=2s → fully restored.
    const float at_two = target_gain_after_silence(2.0f, p);
    EXPECT_GT(at_two, -0.5f);
    // Monotone non-decreasing over the restore window.
    float last = at_zero;
    for (float t = 0.0f; t <= 2.0f; t += 0.1f) {
        const float g = target_gain_after_silence(t, p);
        EXPECT_GE(g, last - 1e-3f) << "t=" << t;
        last = g;
    }
}
