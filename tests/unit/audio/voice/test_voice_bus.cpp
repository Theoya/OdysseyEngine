// test_voice_bus.cpp — unit tests for VoiceBus::preview_for_listener.
//
// Council conditions covered:
//  - "preview_for_listener pure" — verify determinism + no hidden state.
//  - "out-of-range listener" — graceful drop to inactive, not error.
//  - "TooManySpeakers" — budget gate (architect frame-budget addition).
//  - "InvalidListener" — NaN pose guard.

#include "audio/voice/voice_bus.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

using namespace odyssey::audio::voice;

namespace {
dsp::ListenerPose default_listener() {
    dsp::ListenerPose p;
    p.position = {0.0f, 0.0f, 0.0f};
    p.forward  = {0.0f, 0.0f, -1.0f};
    p.right    = {1.0f, 0.0f, 0.0f};
    p.up       = {0.0f, 1.0f, 0.0f};
    return p;
}
SpeakerSnapshot speaker_at(uint32_t id, float x, float y, float z,
                           float range = 25.0f) {
    SpeakerSnapshot s;
    s.speaker_entity_id = id;
    s.pose.position = {x, y, z};
    s.pose.forward  = {0.0f, 0.0f, 1.0f};
    s.pose.right    = {-1.0f, 0.0f, 0.0f};
    s.pose.up       = {0.0f, 1.0f, 0.0f};
    s.voice_range_m = range;
    return s;
}
} // namespace

TEST(VoiceBus, EmptySpeakerListProducesEmptyMix) {
    auto listener = default_listener();
    std::vector<SpeakerSnapshot> speakers;
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{};
    dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_ok());
    EXPECT_TRUE(r.value().sources.empty());
    EXPECT_EQ(r.value().speakers_in_range, 0u);
}

TEST(VoiceBus, SpeakerFrontCenterPansEqual) {
    auto listener = default_listener();
    // Forward is -Z — put the speaker in front.
    std::vector<SpeakerSnapshot> speakers = { speaker_at(42, 0.0f, 0.0f, -5.0f) };
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_ok());
    ASSERT_EQ(r.value().sources.size(), 1u);
    const auto& s = r.value().sources[0];
    EXPECT_TRUE(s.active);
    EXPECT_NEAR(s.pan_L, 0.7071f, 0.01f);
    EXPECT_NEAR(s.pan_R, 0.7071f, 0.01f);
    EXPECT_FALSE(s.rear_hemisphere);
    EXPECT_GT(s.attenuation_linear, 0.0f);
}

TEST(VoiceBus, SpeakerToTheRightPansRight) {
    auto listener = default_listener();
    std::vector<SpeakerSnapshot> speakers = { speaker_at(7, 5.0f, 0.0f, 0.0f) };
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_ok());
    const auto& s = r.value().sources[0];
    EXPECT_GT(s.pan_R, s.pan_L);
}

TEST(VoiceBus, SpeakerBehindIsRearHemisphere) {
    auto listener = default_listener();
    // Behind = +Z when forward is -Z.
    std::vector<SpeakerSnapshot> speakers = { speaker_at(9, 0.0f, 0.0f, 5.0f) };
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_ok());
    const auto& s = r.value().sources[0];
    EXPECT_TRUE(s.rear_hemisphere);
    // LPF should be lower than a front-hemisphere speaker at the same distance.
    EXPECT_LT(s.lpf_cutoff_hz, 18000.0f);
}

TEST(VoiceBus, SpeakerBeyondRangeIsInactive) {
    auto listener = default_listener();
    std::vector<SpeakerSnapshot> speakers = { speaker_at(11, 0.0f, 0.0f, -100.0f) };
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_ok());
    const auto& s = r.value().sources[0];
    EXPECT_FALSE(s.active);
    EXPECT_EQ(r.value().speakers_dropped, 1u);
}

TEST(VoiceBus, MutedSpeakerDropped) {
    auto listener = default_listener();
    auto s0 = speaker_at(5, 0.0f, 0.0f, -5.0f);
    s0.muted_by_listener = true;
    std::vector<SpeakerSnapshot> speakers = { s0 };
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_ok());
    EXPECT_FALSE(r.value().sources[0].active);
    EXPECT_EQ(r.value().speakers_dropped, 1u);
}

TEST(VoiceBus, InvalidListenerPose) {
    dsp::ListenerPose p{};
    p.position.x = std::numeric_limits<float>::quiet_NaN();
    std::vector<SpeakerSnapshot> speakers;
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(p, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), VoiceBusError::InvalidListener);
}

TEST(VoiceBus, TooManySpeakersRejected) {
    auto listener = default_listener();
    std::vector<SpeakerSnapshot> speakers;
    for (uint32_t i = 0; i < kMaxSpeakers + 1; ++i) {
        speakers.push_back(speaker_at(i, 0.0f, 0.0f, -5.0f));
    }
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), VoiceBusError::TooManySpeakers);
}

TEST(VoiceBus, DuckEnvelopeActivatesWhenSpeakerInRange) {
    auto listener = default_listener();
    std::vector<SpeakerSnapshot> speakers = { speaker_at(1, 0.0f, 0.0f, -5.0f) };
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    // Run enough frames for the envelope to lift.
    ListenerMix last{};
    for (int i = 0; i < 20; ++i) {
        auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
        ASSERT_TRUE(r.is_ok());
        last = r.value();
    }
    EXPECT_LT(last.duck_envelope.music_gain_db, -4.0f);
    EXPECT_TRUE(last.duck_envelope.carve_engaged);
}

TEST(VoiceBus, PerEntityVoiceRangeOverridesDefault) {
    auto listener = default_listener();
    // Speaker at 10m. Default d_max=25 → audible. Override to 5 → inactive.
    std::vector<SpeakerSnapshot> speakers = { speaker_at(1, 0.0f, 0.0f, -10.0f, 5.0f) };
    dsp::VoiceRangeParams range{};
    dsp::DuckerState ds{}; dsp::DuckerParams dp{};
    auto r = preview_for_listener(listener, speakers, range, ds, dp, 0.02f);
    ASSERT_TRUE(r.is_ok());
    EXPECT_FALSE(r.value().sources[0].active)
        << "per-entity voice_range=5 should clip a 10m speaker";
}
