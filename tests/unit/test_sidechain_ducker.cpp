#include <gtest/gtest.h>
#include "audio/music/detail/sidechain_ducker.h"
#include <cmath>

using odyssey::audio::music::detail::SidechainDuckerState;
using odyssey::audio::music::detail::SidechainDuckerParams;
using odyssey::audio::music::detail::tick_sidechain_ducker;

TEST(SidechainDuckerTest, InitialState) {
    SidechainDuckerState state;
    EXPECT_EQ(state.current_gain_linear, 1.0f);
}

TEST(SidechainDuckerTest, SmoothingTowardTarget) {
    SidechainDuckerState state;
    SidechainDuckerParams params;
    params.gain_target_when_active_db = -6.0f;
    params.gain_target_idle_db = 0.0f;
    params.time_constant_s = 2.0f;

    float dt = 0.1f;
    tick_sidechain_ducker(state, true, false, dt, params);
    // Should have moved toward -6dB = 0.5012 linear
    EXPECT_LT(state.current_gain_linear, 1.0f);
    EXPECT_GT(state.current_gain_linear, 0.5f);
}

TEST(SidechainDuckerTest, SilenceAsAPrimitive) {
    SidechainDuckerState state;
    state.current_gain_linear = 0.0f;
    SidechainDuckerParams params;
    params.gain_target_when_active_db = -80.0f;  // ~0.0001 linear
    params.gain_target_idle_db = -80.0f;
    params.time_constant_s = 0.1f;  // Very fast

    float dt = 0.5f;
    tick_sidechain_ducker(state, false, false, dt, params);
    // Should remain very close to 0.
    EXPECT_LT(state.current_gain_linear, 0.001f);
}

TEST(SidechainDuckerTest, StingerPriorityFloor) {
    SidechainDuckerState state;
    SidechainDuckerParams params;
    params.gain_target_when_active_db = -60.0f;  // Deep ducking
    params.stinger_priority_floor_db = -3.0f;
    params.time_constant_s = 1.0f;

    // With stinger active, gain should not go below -3dB (0.7079 linear)
    tick_sidechain_ducker(state, true, true, 0.1f, params);
    float expected_floor_linear = std::pow(10.0f, -3.0f / 20.0f);
    EXPECT_GE(state.current_gain_linear, expected_floor_linear - 0.01f);
}
