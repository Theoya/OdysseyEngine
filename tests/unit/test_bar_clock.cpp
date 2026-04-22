#include <gtest/gtest.h>
#include "audio/music/detail/bar_clock.h"

using odyssey::audio::music::detail::BarClock;
using odyssey::audio::music::detail::QuantizeMode;

TEST(BarClockTest, InitialState) {
    BarClock clock(120.0f, 48000);
    EXPECT_EQ(clock.bar(), 0);
    EXPECT_EQ(clock.beat(), 0);
    EXPECT_NEAR(clock.phase(), 0.0f, 0.001f);
}

TEST(BarClockTest, TickAdvancesBar) {
    BarClock clock(120.0f, 48000);
    // At 120 BPM, 48kHz: samples_per_bar = 48000 * 60 / 120 * 4 = 96000 samples
    // = 2.0 seconds per bar
    float dt = 2.0f;
    clock.tick(dt);
    EXPECT_EQ(clock.bar(), 1);
    EXPECT_EQ(clock.beat(), 0);
    EXPECT_NEAR(clock.phase(), 0.0f, 0.001f);
}

TEST(BarClockTest, TickAdvancesPhase) {
    BarClock clock(120.0f, 48000);
    float dt = 0.5f;  // Half second into a 2-second bar
    clock.tick(dt);
    EXPECT_EQ(clock.bar(), 0);
    EXPECT_NEAR(clock.phase(), 0.25f, 0.001f);  // 25% through the bar
}

TEST(BarClockTest, MultipleTicks) {
    BarClock clock(120.0f, 48000);
    clock.tick(1.0f);
    EXPECT_EQ(clock.bar(), 0);
    clock.tick(1.0f);
    EXPECT_EQ(clock.bar(), 1);  // 2 seconds total = 1 bar
}

TEST(BarClockTest, QuantizeImmediate) {
    BarClock clock(120.0f, 48000);
    clock.tick(0.5f);
    uint64_t target = clock.quantize_to_boundary(QuantizeMode::Immediate, 4);
    // Current position: after 0.5 seconds at 120 BPM
    // samples_per_bar = 96000, so 0.5s = 24000 samples into bar 0
    // Immediate should return current beat/bar position (rounded up)
    EXPECT_EQ(target, 48000);  // 0 bar + 2 beats = 48000 samples
}

TEST(BarClockTest, QuantizeNextBar) {
    BarClock clock(120.0f, 48000);
    clock.tick(0.5f);  // 24000 samples
    uint64_t target = clock.quantize_to_boundary(QuantizeMode::NextBar, 4);
    // samples_per_bar = 96000, next boundary is 96000
    EXPECT_EQ(target, 96000);
}
